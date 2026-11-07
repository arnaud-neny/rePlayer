// SPDX-License-Identifier: MIT
// Richard Joseph Player: the replayer of the eagleplayer "RichardJoseph_Player". Four voices, each running a list of
// patterns once per tick: notes start an instrument with a volume envelope and optional pitch and volume LFOs; the
// driver writes the Paula registers itself, DMA one tick after a note and the loop registers one more tick later.

#include "rjp_internal.h"
#include <string.h>

#define COMMAND_FLAG 0x80
#define COMMAND_MASK 0x7F
#define PERIOD_ENTRIES 36
#define SEMITONES_PER_OCTAVE 12
#define FIRST_OCTAVE 1 // semitones from C-0 of the table's first octave: its C is C-1 (period 856)
#define START_SPEED 6
#define MASTER_VOLUME 64
#define MAX_VOLUME 64
#define VOLUME_SHIFT 6
#define LFO_SHIFT 7
#define MAX_ROW_STEPS 65536 // commands and pattern changes one row may take; a longer chain never ends

// The commands: a byte with bit 7 set, the command number in the low bits.
typedef enum RjpCommand {
    RjpCommand_NextPattern = 0, // the pattern ends
    RjpCommand_Release = 1,     // the envelope starts its release; ends the row
    RjpCommand_Speed = 2,       // ticks per count
    RjpCommand_RowLength = 3,   // counts per row
    RjpCommand_Instrument = 4,  // 0 keeps the instrument
    RjpCommand_Volume = 5,      // the instrument's volume; one more byte follows that is not used
    RjpCommand_Slide = 6,       // pitch slide: ticks and a 32-bit step
    RjpCommand_EndRow = 7,
    RjpCommand_ReleaseToo = 8, // the jump table is one entry short: command 8 lands on the release handler
} RjpCommand;

// The periods of the notes, indexed by half the note byte: three octaves, each from its B down to its C.
static const uint16_t s_periods[PERIOD_ENTRIES]
    = { 453, 480, 508, 538, 570, 604, 640, 678, 720, 762, 808, 856, 226, 240, 254, 269, 285, 302,
        320, 339, 360, 381, 404, 428, 113, 120, 127, 135, 143, 151, 160, 170, 180, 190, 202, 214 };

// AUDxLC ignores its lowest bit; AUDxLEN is written as the instrument has it (0 plays 65536 words). The image is a
// nuade_paula_padded_copy, so a sample inside it that runs past its end plays zeros. A start outside the image (a
// sample that ends beyond the 68000's address space, see samples_end) points nowhere.

static void set_sample(const RjpImage* samples, NuadePaulaChannel* paula, uint32_t offset, uint16_t words) {
    offset &= ~1u;
    paula->lc = offset < samples->size ? (const int8_t*)(samples->data + offset) : nullptr;
    paula->len_words = words;
}

static void stop_voice(RjpPlayer* player, RjpVoice* voice) {
    if (voice->active) {
        voice->active = false;
        player->voices_active--;
    }
}

// The eagleplayer's audio interrupt hook, run when a voice's pattern list wraps or stops: when every voice that has a
// list has done so, SongEnd is called and the marks start over.

static void mark_wrapped(RjpPlayer* player, uint32_t index) {
    player->wrap_pending[index] = false;
    for (uint32_t i = 0; i < RJP_VOICES; i++) {
        if (player->wrap_pending[i]) {
            return;
        }
    }
    memcpy(player->wrap_pending, player->wrap_initial, sizeof(player->wrap_pending));
    player->looped = true;
}

// The pattern list ends: the next entry is 0 and then 0 (stop), a distance back (loop) or a negative byte followed by a
// sequence number (jump). Sets `*pattern_pos` to the next pattern's commands; false when the voice stops.

static bool next_pattern(RjpPlayer* player, RjpVoice* voice, uint32_t index, uint32_t* pattern_pos) {
    const RjpModule* module = &player->module;
    const RjpImage* image = &module->image;
    voice->row_length = 1;
    uint32_t list = voice->sequence_pos;
    for (uint32_t step = 0; step < MAX_ROW_STEPS; step++) {
        const uint8_t pattern = rjp_u8(image, list++);
        if (pattern != 0) {
            voice->sequence_pos = list;
            *pattern_pos = rjp_pattern_start(module, pattern);
            return true;
        }
        mark_wrapped(player, index);
        const uint8_t next = rjp_u8(image, list);
        if (next == 0) {
            return false;
        }
        if (next < COMMAND_FLAG) {
            list -= next;
        } else {
            list = rjp_sequence_start(module, rjp_u8(image, (uint64_t)list + 1));
        }
    }
    return false;
}

// Instrument change: the voice takes the instrument's volume, LFOs and loop. The instrument that is already selected
// stays as it is (its LFOs keep their positions).

static void select_instrument(RjpPlayer* player, RjpVoice* voice, uint8_t number) {
    if (number == voice->instrument_number) {
        return;
    }
    const RjpInstrument in = rjp_read_instrument(&player->module, number);
    voice->instrument_number = number;
    voice->has_instrument = true;
    voice->instrument = in;
    voice->volume = in.volume;
    voice->pitch_lfo = (RjpLfo) { .enabled = in.has_pitch_lfo,
                                  .table = in.pitch_lfo,
                                  .restart = in.pitch_lfo_restart * 2u,
                                  .end = in.pitch_lfo_words * 2u,
                                  .position = 0 };
    voice->volume_lfo = (RjpLfo) { .enabled = in.has_volume_lfo,
                                   .table = in.volume_lfo,
                                   .restart = in.volume_lfo_restart * 2u,
                                   .end = in.volume_lfo_words * 2u,
                                   .position = 0 };
}

// The envelope ramps from `target - step` to `target` in `duration + 1` ticks.

static void start_ramp(RjpEnvelopeState* envelope, RjpEnvelopeStage stage, uint8_t from, uint8_t to, uint8_t ticks) {
    envelope->stage = stage;
    envelope->target = to;
    envelope->step = (int8_t)(uint8_t)(to - from);
    envelope->duration = ticks;
    envelope->remaining = ticks;
}

static void start_envelope(const RjpModule* module, RjpVoice* voice) {
    RjpEnvelopeState* envelope = &voice->envelope;
    envelope->has_block = true;
    envelope->block = rjp_envelope_block(module, &voice->instrument);
    const RjpEnvelopeShape shape = rjp_read_envelope(module, envelope->block);
    start_ramp(envelope, RjpEnvelopeStage_Attack, shape.start, shape.peak, shape.attack_ticks);
}

// The release command: a ramp from the current level to 0.

static void release_envelope(const RjpModule* module, RjpVoice* voice) {
    RjpEnvelopeState* envelope = &voice->envelope;
    if (!envelope->has_block) {
        return;
    }
    const RjpEnvelopeShape shape = rjp_read_envelope(module, envelope->block);
    start_ramp(envelope, RjpEnvelopeStage_Release, envelope->level, 0, shape.release_ticks);
}

// A note: stops the channel, clears the first word of the attack, points the channel at the attack and starts the
// envelope. DMA goes on in the next tick. A voice without an instrument plays nothing.

static void start_note(RjpPlayer* player, RjpVoice* voice, uint16_t period) {
    if (!voice->has_instrument) {
        return;
    }
    const RjpInstrument* in = &voice->instrument;
    voice->period = period;
    voice->lfo_period = period;
    if (!voice->slide_keep) {
        voice->slide_pos = 0;
        voice->slide_ticks = 0;
    }
    start_envelope(&player->module, voice);
    nuade_paula_channel_dma_off(&voice->paula);
    const uint32_t attack = in->sample + in->attack_offset;
    if ((uint64_t)attack + 2 <= player->samples.size) {
        player->samples.data[attack] = 0;
        player->samples.data[attack + 1] = 0;
    }
    set_sample(&player->samples, &voice->paula, attack, in->attack_words);
    voice->dma_pending = true;
}

// The note byte indexes the period table in bytes; a byte outside the table reads the driver's variables, which no
// song does. It plays no period.

static uint16_t note_period(uint8_t note) {
    const uint32_t entry = note / 2u;
    return (note & 1) == 0 && entry < PERIOD_ENTRIES ? s_periods[entry] : 0;
}

// Semitones from C-0: the table's entries run down from the B of each octave.

static int note_to_semitone(uint8_t note) {
    const uint32_t entry = note / 2u;
    if ((note & 1) != 0 || entry >= PERIOD_ENTRIES) {
        return -1;
    }
    return (int)((FIRST_OCTAVE + entry / SEMITONES_PER_OCTAVE) * SEMITONES_PER_OCTAVE
                 + (SEMITONES_PER_OCTAVE - 1 - entry % SEMITONES_PER_OCTAVE));
}

// The pitch slide command: 1 byte of ticks, then the step as a big-endian long.

static void start_slide(const RjpImage* image, RjpVoice* voice, uint32_t* pos) {
    voice->slide_keep = true;
    voice->slide_ticks = rjp_u8(image, (*pos)++);
    voice->slide_step = rjp_u32(image, *pos);
    *pos += 4;
    voice->slide_pos = 0;
}

// What a command does to the voice. Returns false when the row ends. A command above 8 jumps into the middle of
// the driver's instructions; the voice stops.

static bool run_command(RjpPlayer* player, RjpVoice* voice, uint32_t index, uint8_t command, uint32_t* pos) {
    const RjpImage* image = &player->module.image;
    switch (command) {
        case RjpCommand_NextPattern:
            if (!next_pattern(player, voice, index, pos)) {
                stop_voice(player, voice);
                return false;
            }
            return true;
        case RjpCommand_Release:
        case RjpCommand_ReleaseToo:
            release_envelope(&player->module, voice);
            return false;
        case RjpCommand_Speed:
            voice->speed = rjp_u8(image, (*pos)++);
            return true;
        case RjpCommand_RowLength:
            voice->row_length = rjp_u8(image, (*pos)++);
            return true;
        case RjpCommand_Instrument: {
            const uint8_t number = rjp_u8(image, (*pos)++);
            if (number != 0) {
                select_instrument(player, voice, number);
            }
            return true;
        }
        case RjpCommand_Volume:
            voice->volume = rjp_u8(image, *pos);
            *pos += 2;
            return true;
        case RjpCommand_Slide:
            start_slide(image, voice, pos);
            return true;
        case RjpCommand_EndRow:
            return false;
        default:
            stop_voice(player, voice);
            return false;
    }
}

// Runs the commands up to the next note or the end of the row.

static void read_row(RjpPlayer* player, RjpVoice* voice, uint32_t index) {
    const RjpImage* image = &player->module.image;
    uint32_t pos = voice->pattern_pos;
    for (uint32_t step = 0; step < MAX_ROW_STEPS; step++) {
        const uint8_t byte = rjp_u8(image, pos++);
        if (byte < COMMAND_FLAG) {
            voice->note = note_to_semitone(byte);
            start_note(player, voice, note_period(byte));
            break;
        }
        if (!run_command(player, voice, index, byte & COMMAND_MASK, &pos)) {
            break;
        }
    }
    voice->pattern_pos = pos;
}

// The sequencer: a count every `speed` ticks, a row every `row_length` counts.

static void run_sequencer(RjpPlayer* player, RjpVoice* voice, uint32_t index) {
    if (--voice->speed_counter != 0) {
        return;
    }
    if (--voice->row_counter == 0) {
        read_row(player, voice, index);
        voice->row_counter = voice->row_length;
    }
    voice->speed_counter = voice->speed;
}

// The tick after a note: the channel's DMA goes on, and it latches the attack.

static void start_dma(RjpVoice* voice) {
    if (voice->dma_pending) {
        voice->dma_pending = false;
        voice->loop_pending = true;
        nuade_paula_channel_dma_on(&voice->paula);
    }
}

// One tick after that: the registers get the loop, which the channel plays when the attack ends.

static void write_loop(const RjpImage* samples, RjpVoice* voice) {
    if (voice->loop_pending) {
        voice->loop_pending = false;
        const RjpInstrument* in = &voice->instrument;
        set_sample(samples, &voice->paula, in->sample + in->loop_offset_words * 2u, in->loop_words);
    }
}

// A stage of the envelope has run its ticks. The attack goes on to the decay to the sustain level, which stays.

static void end_envelope_stage(const RjpModule* module, RjpEnvelopeState* envelope) {
    if (envelope->stage != RjpEnvelopeStage_Attack) {
        envelope->stage = RjpEnvelopeStage_Idle;
        return;
    }
    const RjpEnvelopeShape shape = rjp_read_envelope(module, envelope->block);
    start_ramp(envelope, RjpEnvelopeStage_Decay, shape.peak, shape.sustain, shape.decay_ticks);
}

// Returns false when the release ends: that tick leaves the volume register as the last tick wrote it (the original
// returns from the envelope routine before it loads the level).

static bool run_envelope(const RjpModule* module, RjpEnvelopeState* envelope) {
    if (envelope->stage == RjpEnvelopeStage_Idle) {
        return true;
    }
    const int32_t covered = envelope->duration == 0 ? 0 : envelope->step * envelope->remaining / envelope->duration;
    envelope->level = (uint8_t)(envelope->target - covered);
    if (envelope->remaining-- == 0) {
        const bool release = envelope->stage == RjpEnvelopeStage_Release;
        end_envelope_stage(module, envelope);
        return !release;
    }
    return true;
}

// The next signed byte of an LFO table; the position wraps back to the restart when it reaches the end.

static int8_t run_lfo(const RjpImage* samples, RjpLfo* lfo) {
    const int8_t value = (int8_t)rjp_u8(samples, (uint64_t)lfo->table + lfo->position);
    lfo->position++;
    if (lfo->position == lfo->end) {
        lfo->position = lfo->restart;
    }
    return value;
}

// word * word >> 6 on the low word of the product, as the 68000 code does it.

static int16_t scale_volume(int16_t value, int16_t factor) {
    return (int16_t)((int16_t)(value * factor) >> VOLUME_SHIFT);
}

// The envelope level, the volume LFO, the instrument volume and the master volume give AUDxVOL.

static void update_volume(const RjpPlayer* player, RjpVoice* voice) {
    if (run_envelope(&player->module, &voice->envelope)) {
        voice->level = voice->envelope.level;
    }
    if (voice->volume_lfo.enabled) {
        const int8_t lfo = run_lfo(&player->samples, &voice->volume_lfo);
        voice->level = (uint16_t)(voice->level + ((int16_t)(lfo * (int16_t)voice->level) >> LFO_SHIFT));
    }
    int16_t volume = scale_volume((int16_t)voice->level, (int16_t)voice->volume);
    volume = scale_volume(volume, MASTER_VOLUME);
    voice->level = (uint16_t)volume;
    // A word above the maximum plays at the maximum, a negative one is silent
    if ((uint16_t)volume > MAX_VOLUME) {
        volume = volume > MAX_VOLUME ? MAX_VOLUME : 0;
    }
    voice->paula.volume = (uint8_t)volume;
}

// The pitch LFO moves the period by a fraction of it (a rise is halved), the slide adds to it every tick.

static void update_pitch(RjpPlayer* player, RjpVoice* voice) {
    if (voice->pitch_lfo.enabled) {
        const int8_t lfo = run_lfo(&player->samples, &voice->pitch_lfo);
        int16_t change = (int16_t)-(int16_t)((lfo * (int16_t)voice->period) >> LFO_SHIFT);
        if (change < 0) {
            change >>= 1;
        }
        voice->lfo_period = (uint16_t)(change + voice->period);
    }
    if (voice->slide_ticks != 0) {
        voice->slide_pos += voice->slide_step;
        voice->slide_ticks--;
    }
    const uint16_t period = (uint16_t)((voice->slide_pos >> 16) + voice->lfo_period);
    voice->paula.period = period;
}

static void run_voice(RjpPlayer* player, uint32_t index) {
    RjpVoice* voice = &player->voices[index];
    voice->slide_keep = false;
    if (!voice->active) {
        return;
    }
    write_loop(&player->samples, voice);
    start_dma(voice);
    run_sequencer(player, voice, index);
    update_volume(player, voice);
    update_pitch(player, voice);
}

// A voice starts on a pattern list: its first pattern is the first entry.

static void start_sequence(RjpPlayer* player, uint32_t index, uint8_t sequence) {
    const RjpModule* module = &player->module;
    RjpVoice* voice = &player->voices[index];
    const uint32_t list = rjp_sequence_start(module, sequence);
    voice->sequence_pos = list + 1;
    voice->pattern_pos = rjp_pattern_start(module, rjp_u8(&module->image, list));
    voice->speed = START_SPEED;
    voice->speed_counter = 1;
    voice->row_counter = 1;
    voice->active = true;
    player->wrap_pending[index] = true;
    player->sequence_count++;
    player->voices_active++;
}

bool rjp_start(RjpPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    memcpy(player->samples.data, player->samples_original, player->samples.size);
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->wrap_pending, 0, sizeof(player->wrap_pending));
    player->sequence_count = 0;
    player->voices_active = 0;
    player->looped = false;
    const RjpModule* module = &player->module;
    for (uint32_t i = 0; i < RJP_VOICES; i++) {
        player->voices[i].note = -1;
        const uint8_t sequence = rjp_song_sequence(module, subsong, i);
        if (sequence != 0) {
            start_sequence(player, i, sequence);
        }
        nuade_voice_stop(voices, i);
    }
    memcpy(player->wrap_initial, player->wrap_pending, sizeof(player->wrap_initial));
    return true;
}

RjpTickResult rjp_tick(RjpPlayer* player, NuadeVoices* voices) {
    // The Interrupt calls SongEnd when the subsong started without a voice
    player->looped = player->sequence_count == 0;
    for (uint32_t i = 0; i < RJP_VOICES; i++) {
        run_voice(player, i);
    }
    for (uint32_t i = 0; i < RJP_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (RjpTickResult) { .looped = player->looped, .ended = player->voices_active == 0 };
}

uint32_t rjp_grid_ticks(void) {
    return START_SPEED;
}

RjpVoiceState rjp_voice_state(const RjpPlayer* player, uint32_t voice) {
    if (voice >= RJP_VOICES) {
        return (RjpVoiceState) { .note = -1, .instrument = -1 };
    }
    const RjpVoice* v = &player->voices[voice];
    return (RjpVoiceState) { .note = v->note, .instrument = v->has_instrument ? v->instrument_number : -1 };
}
