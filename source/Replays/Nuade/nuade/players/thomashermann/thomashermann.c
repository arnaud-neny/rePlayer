// SPDX-License-Identifier: MIT
// Thomas Hermann player. Three things run the replayer:
//
//  - the sequencer interrupt (the module's timer value): a counter of ticks per row. The tick before a row reads the
//    row ahead and stops the voices that get a new note; the row's tick runs the commands. It is the eagleplayer's
//    Interrupt, and the position change at the end of the last row is its SongEnd.
//  - the envelope timer (CIA timer B, 25.8 Hz): slides the periods of sliding notes and, every fourth run, steps the
//    volume envelopes.
//  - the audio interrupts of the four channels. They chain the sample blocks of a note: with loop flags 0 the third
//    interrupt stops the DMA; with loop flags the second interrupt writes the instrument's loop as the repeat. A
//    channel whose DMA was switched off raises its request once more, and the request stays while the interrupt is
//    disabled, so the next note on the channel finds one pending and its first real interrupt is already the second.
//
// The audio interrupts run from the voices' block starts: the DMA start answers at once, the later ones at the start of
// the next tick (the mixer counts them). The interrupt request stays set while the interrupt is disabled, as on Paula.

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#include "thm_internal.h"

// The envelope timer is loaded with DeliTracker's default timer word (14187) with its bytes swapped (the replayer
// copies the word's high byte to the low latch), and a CIA timer takes latch + 1 clocks. The handler runs 4 times
// between two envelope steps. Both timers start with the song.
#define ENVELOPE_TIMER (0x6b37 + 1)
#define ENVELOPE_EVERY 4

// The level the envelope-less loop notes hold: 0x40 in the high byte, which is the full instrument volume.
#define FULL_LEVEL 0x4000

// A note with loop flags 0 ends at its third audio interrupt.
#define ONE_SHOT_BLOCK_STARTS 2

ThmCreateResult thm_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    ThmCreateResult result = { .status = ThmStatus_InvalidData };
    ThmModule* module = thm_module_load(data, size, path, io, &result.status);
    if (module == nullptr) {
        return result;
    }
    ThmPlayer* p = calloc(1, sizeof(ThmPlayer));
    if (p == nullptr) {
        thm_module_free(module);
        result.status = ThmStatus_OutOfMemory;
        return result;
    }
    p->module = module;
    result.player = p;
    return result;
}

void thm_destroy(ThmPlayer* player) {
    if (player != nullptr) {
        thm_module_free(player->module);
        free(player);
    }
}

uint32_t thm_subsong_count(const ThmPlayer* player) {
    return player->module->subsong_count;
}

// AUDxVOL as the replayer writes it, after the eagleplayer's volume scaling (the identity at full volume).

static void write_volume(ThmChannel* c, uint32_t volume) {
    c->paula.volume = (uint8_t)(volume & 0x7f);
}

// The volume of a voice that follows the envelope: the instrument volume scaled by the level.

static void write_level_volume(ThmChannel* c, uint8_t level) {
    write_volume(c, ((uint32_t)c->note_volume * level) >> 6);
}

// DMACON clear. A channel that plays raises its interrupt request once more as the sample it still has in its output
// register ends (UAE's audio state machine does this for a channel without DMA), so the request is pending afterwards.

static void switch_dma_off(ThmChannel* c) {
    if (c->paula.dma) {
        c->irq_pending = true;
    }
    nuade_paula_channel_dma_off(&c->paula);
}

// The audio interrupt of a channel.

static void audio_interrupt(const ThmModule* module, ThmChannel* c) {
    c->irq_pending = false;
    if (c->loop_flags == 0) {
        if (c->irq_count < ONE_SHOT_BLOCK_STARTS) {
            c->irq_count++;
        } else {
            c->irq_enabled = false;
            c->irq_count = 0;
            switch_dma_off(c);
        }
    } else if (c->irq_count != 1) {
        c->irq_count++;
    } else {
        c->paula.lc = thm_module_chip(module, c->loop_sample);
        c->paula.len_words = c->loop_words;
        c->irq_enabled = false;
    }
}

static void raise_audio_interrupt(const ThmModule* module, ThmChannel* c) {
    c->irq_pending = true;
    if (c->irq_enabled) {
        audio_interrupt(module, c);
    }
}

// INTENA set: a request that came in while the interrupt was off is taken now.

static void enable_audio_interrupt(const ThmModule* module, ThmChannel* c) {
    c->irq_enabled = true;
    if (c->irq_pending) {
        audio_interrupt(module, c);
    }
}

// Block starts the mixer played since the last tick: one request each. Once the handler has disabled the interrupt,
// the rest only leave the request pending, so the loop ends there.

static void answer_audio_interrupts(const ThmModule* module, ThmChannel* c, uint32_t voice, const NuadeVoices* voices) {
    NuadeVoiceState state;
    nuade_voice_get_state(voices, voice, &state);
    uint32_t block_starts = state.wraps - c->wraps_seen;
    c->wraps_seen = state.wraps;
    while (block_starts > 0 && c->irq_enabled) {
        raise_audio_interrupt(module, c);
        block_starts--;
    }
    if (block_starts > 0) {
        c->irq_pending = true;
    }
}

// The voice stops: DMA and its interrupt off, no envelope work. A pending request stays.

static void stop_voice(ThmChannel* c) {
    switch_dma_off(c);
    c->irq_enabled = false;
    c->envelope_flags = 0;
}

// The first volume of a note, and what the envelope handler needs to follow it.

static void set_up_envelope(ThmChannel* c, const ThmNote* n, const ThmInstrument* instrument) {
    uint32_t volume = n->volume;
    c->envelope_flags = n->flags;
    c->loop_flags = n->flags;
    if (n->flags != 0) {
        c->loop_sample = n->sample + n->loop_offset;
        c->loop_words = n->loop_bytes >> 1;
        c->stage = ThmStage_Attack;
        c->note_volume = n->volume;
        if ((n->flags & THM_ENVELOPE_FLAG) == 0) {
            c->level = FULL_LEVEL;
        } else if (instrument->steps[0] != 0) {
            volume = 0; // the attack starts from silence: the envelope handler's first step sets the slope
        } else {
            c->slope = 0;
            c->stage = ThmStage_First;
            c->level = (uint16_t)(instrument->levels[0] << 8);
            write_level_volume(c, instrument->levels[0]);
            return;
        }
    }
    write_volume(c, volume);
}

// Start the DMA of a note: the audio interrupt is enabled first, so a request left over from the previous note's loop
// is answered before the block start of this one.

static void start_dma(const ThmModule* module, ThmChannel* c) {
    c->irq_count = 0;
    enable_audio_interrupt(module, c);
    const bool was_running = c->paula.dma;
    nuade_paula_channel_dma_on(&c->paula);
    if (!was_running) {
        raise_audio_interrupt(module, c);
    }
}

// A note or a retrigger on a voice.

static void start_note(const ThmModule* module, ThmChannel* c, uint8_t note, ThmRow row) {
    if (row.instrument > thm_module_instrument_count(module)) {
        return;
    }
    const ThmInstrument instrument = thm_module_instrument(module, row.instrument);
    ThmNote n;
    if (!thm_note_build(note, &instrument, &n)) {
        return;
    }
    c->instrument = row.instrument;
    c->paula.lc = thm_module_chip(module, n.sample);
    c->paula.len_words = n.length >> 1;
    const uint16_t period = note == ThmCommand_Retrigger ? n.base_period : n.period;
    c->target_period = period;
    c->note_period = period;
    c->sliding = true;
    if (!row.slide) {
        c->sliding = false;
        c->paula.period = period;
        c->period = period;
    }
    c->release = false;
    set_up_envelope(c, &n, &instrument);
    start_dma(module, c);
}

static void run_row_command(const ThmModule* module, ThmChannel* c) {
    const ThmRow row = c->row;
    switch (row.note) {
        case ThmCommand_None:
            break;
        case ThmCommand_Release:
            c->release = true;
            break;
        case ThmCommand_Stop:
            stop_voice(c);
            break;
        default:
            start_note(module, c, row.note, row);
            break;
    }
}

// The tick before a row: read the voice's row from its pattern (transposed) and stop the voice if the row starts a
// note.

static void read_row_ahead(ThmPlayer* p, uint32_t voice) {
    const ThmModule* m = p->module;
    ThmChannel* c = &p->channel[voice];
    const ThmRow raw = thm_module_row(m, voice, p->position, p->row);
    const uint8_t note = thm_transpose_note(raw.note, thm_module_transposition(m, voice, p->position));
    c->row = (ThmRow) { .note = note, .instrument = raw.instrument, .slide = raw.slide, .no_stop = raw.no_stop };
    if (raw.no_stop) {
        return;
    }
    if (note < THM_COMMAND_NOTES || note == ThmCommand_Retrigger) {
        stop_voice(c);
    }
}

// The tick of a row: the commands of all voices, then the next row; after the last row the next position, or the song
// start again.

static void run_row(ThmPlayer* p) {
    p->tick_counter = (int8_t)p->speed;
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        if (!thm_module_voice_off(p->module, v)) {
            run_row_command(p->module, &p->channel[v]);
        }
    }
    const uint8_t row = p->row++;
    if (row == thm_module_last_row(p->module)) {
        p->row = 0;
        if (p->position == p->last_position) {
            p->position = p->loop_position;
            p->looped = true;
        } else {
            p->position++;
        }
    }
}

static void run_sequencer(ThmPlayer* p) {
    if (p->tick_counter == 0) {
        for (uint32_t v = 0; v < THM_VOICES; v++) {
            read_row_ahead(p, v);
        }
        p->tick_counter--;
    } else if (p->tick_counter < 0) {
        run_row(p);
    } else {
        p->tick_counter--;
    }
}

// A signed 32-bit by 16-bit division with the 68000's overflow rule: a quotient that does not fit leaves the
// destination, whose low word is what the replayer stores.

static int16_t divide_low_word(int32_t dividend, int16_t divisor) {
    const int32_t quotient = dividend / divisor;
    if (quotient < INT16_MIN || quotient > INT16_MAX) {
        return (int16_t)dividend;
    }
    return (int16_t)quotient;
}

// One step of the level towards `target` along the slope. On arriving, the level is the target and the slope for the
// next segment (to `next`, over `steps` steps) is worked out.

static void step_level(ThmChannel* c, uint8_t target, uint8_t next, uint8_t steps) {
    c->level = (uint16_t)(c->level + (uint16_t)c->slope);
    const int8_t level = (int8_t)(c->level >> 8);
    const bool arrived = c->slope < 0 ? level <= (int8_t)target : level >= (int8_t)target;
    if (!arrived) {
        write_level_volume(c, (uint8_t)level);
        return;
    }
    c->level = (uint16_t)(target << 8);
    write_level_volume(c, target);
    c->slope = divide_low_word(((int32_t)next - (int32_t)target) * 256, steps);
    c->stage = (ThmStage)(c->stage + 1);
}

// The release (command 0xdf): the level falls to 0 over the instrument's last segment time.

static void release_voice(ThmChannel* c, const ThmInstrument* instrument) {
    if (c->stage != ThmStage_Release) {
        c->stage = ThmStage_Release;
        const uint8_t steps = instrument->steps[3] != 0 ? instrument->steps[3] : 1;
        c->slope = divide_low_word(-(int32_t)c->level, steps);
    }
    step_level(c, 0, 0, 1);
    if (c->stage == ThmStage_Done) {
        c->envelope_flags = 0;
    }
}

// The envelope handler for one voice: the attack and three more segments with the instrument's levels and step counts.

static void step_envelope(ThmPlayer* p, uint32_t voice) {
    ThmChannel* c = &p->channel[voice];
    if (c->envelope_flags == 0) {
        return;
    }
    const ThmInstrument instrument = thm_module_instrument(p->module, c->instrument);
    if (c->release) {
        release_voice(c, &instrument);
        return;
    }
    if ((c->envelope_flags & THM_ENVELOPE_FLAG) == 0) {
        return;
    }
    uint8_t target = 0;
    uint8_t next = 0;
    uint8_t steps = 0;
    switch (c->stage) {
        case ThmStage_Attack:
            c->slope = (int16_t)(((uint32_t)instrument.levels[0] << 8) / instrument.steps[0]);
            c->level = (uint16_t)c->slope;
            c->stage = ThmStage_First;
            write_level_volume(c, (uint8_t)(c->level >> 8));
            return;
        case ThmStage_First:
            target = instrument.levels[0];
            next = instrument.levels[1];
            steps = instrument.steps[1];
            break;
        case ThmStage_Second:
            target = instrument.levels[1];
            next = instrument.levels[2];
            steps = instrument.steps[2];
            break;
        case ThmStage_Third:
            target = instrument.levels[2];
            next = 0;
            steps = instrument.steps[3];
            break;
        default:
            return;
    }
    step_level(c, target, next, steps != 0 ? steps : 1);
}

// A sliding note moves its period towards the target by the instrument's speed.

static void slide_period(ThmPlayer* p, uint32_t voice) {
    ThmChannel* c = &p->channel[voice];
    if (!c->sliding) {
        return;
    }
    const uint16_t speed = thm_module_instrument(p->module, c->instrument).slide_speed;
    uint16_t period = c->period;
    if (period < c->target_period) {
        period = (uint16_t)(period + speed);
        if (period >= c->target_period) {
            period = c->target_period;
            c->sliding = false;
        }
    } else if (period > c->target_period) {
        period = (uint16_t)(period - speed);
        if (period <= c->target_period) {
            period = c->target_period;
            c->sliding = false;
        }
    } else {
        c->sliding = false;
    }
    c->period = period;
    c->paula.period = period;
}

// The envelope timer handler: every run slides the periods, every fourth run steps the envelopes.

static void run_envelope_handler(ThmPlayer* p) {
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        slide_period(p, v);
    }
    if (p->envelope_counter != 0) {
        p->envelope_counter--;
        return;
    }
    p->envelope_counter = ENVELOPE_EVERY - 1;
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        step_envelope(p, v);
    }
}

bool thm_start(ThmPlayer* p, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    p->sample_rate = sample_rate;
    if (subsong >= p->module->subsong_count) {
        return false;
    }
    const ThmSubsong song = thm_module_subsong(p->module, subsong);
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        p->channel[v] = (ThmChannel) { 0 };
        NuadeVoiceState state;
        nuade_voice_stop(voices, v);
        nuade_voice_get_state(voices, v, &state);
        p->channel[v].wraps_seen = state.wraps;
    }
    p->loop_position = song.loop_position;
    p->last_position = song.last_position;
    p->position = song.position;
    p->speed = song.speed;
    p->tick_counter = (int8_t)song.speed;
    p->row = 0;
    p->envelope_counter = ENVELOPE_EVERY - 1;
    p->sequencer_period = (uint32_t)thm_module_timer(p->module) + 1; // a CIA timer takes latch + 1 clocks
    p->next_sequencer = p->sequencer_period;
    p->next_envelope = ENVELOPE_TIMER;
    p->time = p->next_sequencer < p->next_envelope ? p->next_sequencer : p->next_envelope;
    p->looped = false;
    return true;
}

// Clocks until the voice's next block start, when its audio interrupt can still act on it (a note ends at the third
// and loops at the second): a tick is due then, so the handler runs when the block starts and not at the next timer
// interrupt. Long when there is none. The tick is two mixer frames late, so that the mixer has counted the wrap (it
// settles a wrap when it mixes the frame after the boundary) when the tick runs.

static uint64_t until_block_start(const ThmChannel* c, uint32_t voice, const NuadeVoices* voices,
                                  uint32_t sample_rate) {
    const uint64_t none = UINT64_MAX / 2;
    NuadeVoiceState state;
    if (!c->irq_enabled || !c->paula.dma) {
        return none;
    }
    nuade_voice_get_state(voices, voice, &state);
    if (!state.playing || state.sample == nullptr || state.pitch_hz <= 0.0) {
        return none;
    }
    const double remaining = (double)state.sample->length - state.position;
    const double seconds = (remaining > 0.0 ? remaining : 0.0) / state.pitch_hz;
    const uint64_t frame = THM_TIMEBASE_HZ / sample_rate + 1;
    return (uint64_t)(seconds * THM_TIMEBASE_HZ) + 2 * frame;
}

ThmTickResult thm_tick(ThmPlayer* p, NuadeVoices* voices) {
    p->looped = false;
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        answer_audio_interrupts(p->module, &p->channel[v], v, voices);
    }
    if (p->next_envelope == p->time) {
        run_envelope_handler(p);
        p->next_envelope += ENVELOPE_TIMER;
    }
    if (p->next_sequencer == p->time) {
        run_sequencer(p);
        p->next_sequencer += p->sequencer_period;
    }
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        nuade_paula_channel_sync(&p->channel[v].paula, v, voices);
    }
    uint64_t next = p->next_sequencer < p->next_envelope ? p->next_sequencer : p->next_envelope;
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        const uint64_t block_start = p->time + until_block_start(&p->channel[v], v, voices, p->sample_rate);
        next = block_start < next ? block_start : next;
    }
    const ThmTickResult result = { .length = (uint32_t)(next - p->time), .looped = p->looped };
    p->time = next;
    return result;
}

ThmVoiceState thm_voice_state(const ThmPlayer* player, uint32_t voice) {
    const ThmChannel* c = &player->channel[voice % THM_VOICES];
    return (ThmVoiceState) { .period = c->note_period, .instrument = c->instrument };
}
