// SPDX-License-Identifier: MIT
// Ben Daglish game sound driver: the tick routine, the audio interrupt and the song start, translated from the driver
// in the modules (the early and the late revision, see bds_internal.h).
//
// Each tick runs the three voices' sequencers and envelopes, then writes the voices' wave forms, periods and DMA to
// Paula. The volume is not written by the tick (early) or only for sample instruments (late): the audio interrupt of a
// channel sets it when a block starts. The tick enables the three interrupts when it is done, so the handler runs when
// the next block of the channel starts, or at once if a block has started since it last ran. That is modelled by the
// block starts the mixer counts for a voice, placed in time from the voice's position in its sample.

#include "bds_internal.h"
#include <string.h>

#define NOTE_OFFSET 9 // added to a track's note byte
#define NOTES_PER_OCTAVE 12
#define SILENCE_WORDS 0x100 // the length of the silent buffer the early driver plays for a muted instrument
#define IDLE_WORDS 4        // AUDxLEN while the voices are stopped
#define PORTAMENTO_MIN_PERIOD 0x7C
#define PORTAMENTO_FOREVER 0xFF
#define ARPEGGIO_DONE 0xFF
#define VIBRATO_HALF_WAVE 8
#define MAX_READER_STEPS 4096 // track and order list bytes one voice reads in a tick before the data is called broken
#define MAP_VOICE_STRIDE 8

#define ORDER_LIST_END 0xFF

typedef enum BdsStep {
    BdsStep_Playing = 0,
    BdsStep_SongEnd,
} BdsStep;

static bool is_late(const BdsPlayer* p) {
    return p->module.revision == BdsRevision_Late;
}

// AUDxLC is a word address: the driver's wave pointers are byte addresses, Paula ignores bit 0.

static void write_audio_block(const BdsPlayer* p, BdsChannel* c, uint32_t lc, uint16_t len_words) {
    const BdsMemory* m = &p->module.memory;
    lc &= ~1u;
    c->paula.lc = lc < m->size ? (const int8_t*)(m->data + lc) : nullptr;
    c->paula.len_words = len_words;
}

// Volume of a voice as the driver writes it: the envelope, doubled (the interrupt) or quadrupled (the tick's sample
// volumes) unless the "unscaled" variable is set, and 0 while the volume is disabled.

static uint16_t scaled_volume(const BdsPlayer* p, int voice, unsigned shift) {
    const BdsMemory* m = &p->module.memory;
    const BdsTables* t = &p->module.tables;
    uint16_t volume = p->voices[voice].volume;
    if (bds_u8(m, t->volume_unscaled) == 0) {
        volume = (uint16_t)(volume << shift);
    }
    const bool enabled = is_late(p) ? bds_u8(m, t->volume_enable) != 0 : bds_u16(m, t->volume_enable) != 0;
    return enabled ? volume : 0;
}

// The late driver caps the volume: the low byte is the smaller of volume and cap. The cap is scaled in the part of
// the code that handles voice 0, so what a run with only voice 1 or 2 caps against is the unscaled cap.

static uint16_t capped_volume(const BdsPlayer* p, int voice, unsigned shift, uint16_t* cap) {
    const BdsMemory* m = &p->module.memory;
    if (voice == 0 && bds_u8(m, p->module.tables.volume_unscaled) == 0) {
        *cap = (uint16_t)(*cap << shift);
    }
    uint16_t volume = scaled_volume(p, voice, shift);
    if ((uint8_t)*cap < (uint8_t)volume) {
        volume = (uint16_t)((volume & 0xFF00) | (*cap & 0xFF));
    }
    return volume;
}

// The audio interrupt. One handler serves the four vectors: whichever interrupt is taken, it services every channel
// whose request is set, enabled or not, so the channels that are due together are served in one run. A serviced
// channel disables its interrupt and clears its request; the tick enables the three interrupts when it is done.

static void irq_service_early(BdsPlayer* p, int voice) {
    BdsChannel* c = &p->channels[voice];
    c->paula.volume = (uint8_t)scaled_volume(p, voice, 1);
    c->irq_enabled = false;
    c->irq_request = false;
}

// The late handler caps the volume. A sample instrument keeps its interrupt enabled through the start of the sample;
// when the sample's first pass is through, it stops the channel.

static void irq_service_late(BdsPlayer* p, int voice, uint16_t* cap) {
    BdsChannel* c = &p->channels[voice];
    BdsVoice* s = &p->voices[voice];
    c->paula.volume = (uint8_t)capped_volume(p, voice, 1, cap);
    if (s->sample_state == BdsSampleState_Playing) {
        s->sample_state = BdsSampleState_Done;
        c->irq_request = false;
        return;
    }
    if (s->sample_state == BdsSampleState_Done) {
        c->paula.volume = 0;
        nuade_paula_channel_dma_off(&c->paula);
    }
    c->irq_enabled = false;
    c->irq_request = false;
}

static bool irq_due(const BdsPlayer* p) {
    for (int v = 0; v < BDS_VOICES; v++) {
        if (p->channels[v].irq_enabled && p->channels[v].irq_request) {
            return true;
        }
    }
    return false;
}

// Runs the handler once if an enabled interrupt has a request.

static void irq_run(BdsPlayer* p) {
    if (!irq_due(p)) {
        return;
    }
    uint16_t cap = bds_u8(&p->module.memory, p->module.tables.volume_cap);
    for (int v = 0; v < BDS_VOICES; v++) {
        if (!p->channels[v].irq_request) {
            continue;
        }
        if (is_late(p)) {
            irq_service_late(p, v, &cap);
        } else {
            irq_service_early(p, v);
        }
    }
}

// A block start of a channel and the output frame it happened at.

typedef struct BdsBlockStart {
    double frame;
    int voice;
} BdsBlockStart;

// A wave of 4 words at period 113 wraps ~120 times in a 20 ms tick. Only the last 64 are kept: each handler run sets
// the volume from the same envelope, so more runs change nothing.
#define BLOCK_STARTS_PER_VOICE 64
#define MAX_BLOCK_STARTS (BDS_VOICES * (BLOCK_STARTS_PER_VOICE + 1))

// The block starts of one voice in the tick that has just run. The mixer counts a voice's wraps and keeps where it is
// in the sample, so the frames of the wraps follow from the position, the pitch and the length of the sample: the last
// wrap was `position` source frames ago, the one before it a sample length earlier, and so on.

static uint32_t collect_voice_starts(BdsPlayer* p, int voice, BdsBlockStart* out) {
    BdsChannel* c = &p->channels[voice];
    NuadeVoiceState state;
    nuade_voice_get_state(p->mixer, (uint32_t)voice, &state);
    uint32_t count = 0;
    uint32_t wraps = state.wraps >= c->irq_wraps ? state.wraps - c->irq_wraps : 0;
    c->irq_wraps = state.wraps;
    if (wraps > BLOCK_STARTS_PER_VOICE) {
        wraps = BLOCK_STARTS_PER_VOICE;
    }
    if (wraps > 0) {
        double interval = 0.0;
        double last = state.frame;
        if (state.sample != nullptr && state.pitch_hz > 0.0 && state.sample->loop_length > 0) {
            interval = state.sample->loop_length / state.pitch_hz * p->sample_rate;
            last = state.frame - state.position / state.pitch_hz * p->sample_rate;
        }
        for (uint32_t i = 0; i < wraps; i++) {
            out[count++] = (BdsBlockStart) { .frame = last - (double)(wraps - 1 - i) * interval, .voice = voice };
        }
    }
    if (c->sample_pass_active && (!c->paula.dma || !state.playing)) {
        // The first pass of a sample is through (or the channel was stopped)
        c->sample_pass_active = false;
        if (c->paula.dma) {
            out[count++] = (BdsBlockStart) { .frame = (double)state.event_frame, .voice = voice };
        }
    }
    return count;
}

static void sort_by_frame(BdsBlockStart* starts, uint32_t count) {
    for (uint32_t i = 1; i < count; i++) {
        const BdsBlockStart key = starts[i];
        uint32_t j = i;
        for (; j > 0 && starts[j - 1].frame > key.frame; j--) {
            starts[j] = starts[j - 1];
        }
        starts[j] = key;
    }
}

// At the start of a tick: each block start of the last one raised the request of its channel, and ran the handler if a
// channel was enabled. The requests that came after the last run stay pending for the enable at the end of the tick.

static void irq_replay_block_starts(BdsPlayer* p) {
    BdsBlockStart starts[MAX_BLOCK_STARTS];
    uint32_t count = 0;
    for (int v = 0; v < BDS_VOICES; v++) {
        count += collect_voice_starts(p, v, starts + count);
    }
    sort_by_frame(starts, count);
    for (uint32_t i = 0; i < count; i++) {
        p->channels[starts[i].voice].irq_request = true;
        irq_run(p);
    }
}

// When the tick has enabled the interrupts: the requests that are pending run the handler at once, and so does the
// start of the DMA a little later.

static void irq_poll_enabled(BdsPlayer* p) {
    irq_run(p);
    bool started = false;
    for (int v = 0; v < BDS_VOICES; v++) {
        if (p->channels[v].paula.dma_started) {
            p->channels[v].irq_request = true;
            started = true;
        }
    }
    if (started) {
        irq_run(p);
    }
}

// Sequencer

static uint8_t track_byte(const BdsPlayer* p, const BdsVoice* s, uint8_t pos) {
    return bds_u8(&p->module.memory, s->track + pos);
}

static uint8_t order_byte(const BdsPlayer* p, const BdsVoice* s) {
    return bds_u8(&p->module.memory, s->order_list + s->order_pos);
}

// The wave form, its length and the period of a note: the octave picks the wave form, the semitone the period.

static void set_pitch(const BdsPlayer* p, BdsVoice* s, uint8_t note) {
    const BdsMemory* m = &p->module.memory;
    const BdsTables* t = &p->module.tables;
    const uint32_t octave = note / NOTES_PER_OCTAVE;
    s->lc = bds_u32(m, s->wave_table + 4 * octave);
    s->len_words = bds_u16(m, t->wave_lengths + 2 * octave);
    s->period = (uint16_t)(bds_u16(m, t->periods + 2 * (note % NOTES_PER_OCTAVE)) + s->detune);
}

// A rate nibble gives the envelope step and the ticks between steps: a table byte below 0x80 is a step every tick, from
// 0x80 on its low bits are the ticks between steps of 1.

static void read_rate(const BdsPlayer* p, uint32_t table, uint8_t nibble, uint8_t* step, uint8_t* interval) {
    const uint8_t rate = bds_u8(&p->module.memory, table + nibble);
    if ((rate & 0x80) == 0) {
        *step = rate;
        *interval = 1;
    } else {
        *step = 1;
        *interval = rate & 0x7F;
    }
}

// An instrument selects the wave forms, the envelope and the vibrato of the notes that follow.

static void select_instrument(BdsPlayer* p, int voice, uint8_t number) {
    const BdsMemory* m = &p->module.memory;
    const BdsTables* t = &p->module.tables;
    BdsVoice* s = &p->voices[voice];
    BdsChannel* c = &p->channels[voice];
    const uint8_t map_index = (uint8_t)((number & 0xF) + MAP_VOICE_STRIDE * voice);
    const BdsInstrument instrument = bds_read_instrument(m, t->instruments + bds_u8(m, t->instrument_map + map_index));

    c->display_instrument = number & 0xF;
    if (is_late(p)) {
        s->sample_state = BdsSampleState_None;
    }
    for (int i = 0; i < BDS_WAVE_TABLES; i++) {
        if ((instrument.wave_flags & (0x10 << i)) != 0) {
            s->wave_table = t->wave_tables[i];
        }
    }
    if (is_late(p) && instrument.is_sample) {
        const BdsSample sample = bds_read_sample(m, bds_u32(m, t->sample_table + 4 * instrument.sample_index));
        s->lc = sample.lc;
        s->len_words = sample.len_words;
        s->period = sample.period;
        s->sample_state = BdsSampleState_Playing;
    }
    s->volume_max = instrument.volume_max;
    if (!is_late(p)) {
        c->plays_wave = !instrument.muted;
    }

    s->attack_step = 0xF;
    s->attack_interval = 1;
    if (instrument.attack_nibble != 0) {
        read_rate(p, t->attack_rates, instrument.attack_nibble, &s->attack_step, &s->attack_interval);
    }
    s->sustain_level = instrument.sustain_level;
    s->decay_step = 0xF;
    s->decay_interval = 1;
    if (instrument.decay_nibble != 0) {
        read_rate(p, t->decay_rates, instrument.decay_nibble, &s->decay_step, &s->decay_interval);
    }
    s->release_step = 0xF;
    s->release_interval = 1;
    if (instrument.release_nibble != 0) {
        read_rate(p, t->decay_rates, instrument.release_nibble, &s->release_step, &s->release_interval);
    }
    s->gate_ticks = instrument.gate_ticks;
    s->vibrato_table = instrument.vibrato;
}

// A note: the pitch (not for a sample, which keeps its own), the envelope and effects restart, and the next byte of the
// track is the note's length in ticks.

static void start_note(BdsPlayer* p, int voice, uint8_t byte) {
    const BdsMemory* m = &p->module.memory;
    const BdsTables* t = &p->module.tables;
    BdsVoice* s = &p->voices[voice];
    BdsChannel* c = &p->channels[voice];

    if (is_late(p) && s->sample_state != BdsSampleState_None) {
        s->sample_state = BdsSampleState_Playing;
    } else {
        s->note = (uint8_t)(byte + NOTE_OFFSET + s->transpose);
        set_pitch(p, s, s->note);
        c->display_note = s->note;
    }
    s->gate = true;
    s->attack_counter = 0;
    s->decay_counter = 0;
    s->release_counter = 0;
    s->volume = 0;
    s->attack_done = false;
    s->decay_done = false;
    s->release_done = false;

    const uint8_t length = track_byte(p, s, (uint8_t)(s->track_pos + 1));
    s->track_pos = (uint8_t)(s->track_pos + 2);
    if (s->vibrato_table != 0) {
        s->vibrato_delay = bds_u8(m, t->vibratos + s->vibrato_table);
        s->vibrato_phase = 1;
        s->vibrato_down = false;
    }
    if (s->arpeggio != 0) {
        const uint8_t entry = bds_u8(m, t->arpeggios + s->arpeggio);
        s->arpeggio_ticks = (uint8_t)(((entry & 0x78) >> 3) + 1);
        s->arpeggio_step = 1;
    }
    s->portamento_delay = s->portamento_delay_reload;
    s->note_ticks = length;
}

// Envelope: attack to the instrument's maximum, decay to the sustain level, and release once the gate closes.

static void run_envelope(BdsVoice* s) {
    if (s->gate) {
        if (!s->attack_done) {
            s->attack_counter++;
            if (s->attack_counter == s->attack_interval) {
                s->attack_counter = 0;
                uint8_t volume = (uint8_t)(s->attack_step + s->volume);
                if ((int8_t)volume >= (int8_t)s->volume_max) {
                    volume = s->volume_max;
                    s->attack_done = true;
                }
                s->volume = volume;
            }
        } else if (!s->decay_done) {
            s->decay_counter++;
            if (s->decay_counter == s->decay_interval) {
                s->decay_counter = 0;
                const uint8_t volume = (uint8_t)(s->volume - s->decay_step);
                if ((int8_t)volume < 0 || (int8_t)volume < (int8_t)s->sustain_level) {
                    s->volume = s->sustain_level;
                    s->decay_done = true;
                } else {
                    s->volume = volume;
                }
            }
        }
    } else if (!s->release_done) {
        s->release_counter++;
        if (s->release_counter == s->release_interval) {
            s->release_counter = 0;
            const uint8_t volume = (uint8_t)(s->volume - s->release_step);
            if ((int8_t)volume < 0) {
                s->volume = 0;
                s->release_done = true;
            } else {
                s->volume = volume;
            }
        }
    }
}

// Vibrato: after a delay, the table gives the period offset for each tick of a half wave; the second half wave goes the
// other way.

static void run_vibrato(const BdsPlayer* p, BdsVoice* s) {
    if (s->vibrato_table == 0) {
        return;
    }
    if (s->vibrato_delay != 0) {
        if ((s->vibrato_delay & 0x80) == 0) {
            s->vibrato_delay--;
            return;
        }
        if ((s->vibrato_delay & 0x7F) == 0) {
            return;
        }
    }
    const uint8_t entry
        = bds_u8(&p->module.memory, p->module.tables.vibratos + (uint8_t)(s->vibrato_phase + s->vibrato_table));
    const bool negative = (entry & 0x80) != 0;
    const uint16_t amount = negative ? (uint8_t)(0 - entry) : entry;
    if (negative == s->vibrato_down) {
        s->period = (uint16_t)(s->period + amount);
    } else {
        s->period = (uint16_t)(s->period - amount);
    }
    s->vibrato_phase++;
    if (s->vibrato_phase == VIBRATO_HALF_WAVE) {
        s->vibrato_phase = 1;
        s->vibrato_down = !s->vibrato_down;
        if (!s->vibrato_down && s->vibrato_delay != 0) {
            s->vibrato_delay--;
        }
    }
}

// Arpeggio: each table byte holds a note offset (bits 3 to 7) and its length in ticks (bits 0 to 2). Byte 0 of the
// entry gives the number of steps and whether it loops.

static void run_arpeggio(BdsPlayer* p, BdsVoice* s) {
    const BdsMemory* m = &p->module.memory;
    const uint32_t table = p->module.tables.arpeggios;
    if (s->arpeggio == 0) {
        return;
    }
    s->arpeggio_ticks--;
    if (s->arpeggio_ticks != 0) {
        return;
    }
    const uint32_t entry = table + s->arpeggio;
    if (s->arpeggio_step == ARPEGGIO_DONE) {
        // Back to the note itself
        s->arpeggio_step--;
        set_pitch(p, s, s->note);
        return;
    }
    const uint8_t step_byte = bds_u8(m, table + (uint8_t)(s->arpeggio + s->arpeggio_step));
    uint8_t value = step_byte;
    if (s->arpeggio_step == 0) {
        const uint8_t length = (step_byte & 0x38) >> 3;
        if (length != 0) {
            value = length;
        }
    }
    s->arpeggio_ticks = value & 7;
    set_pitch(p, s, (uint8_t)(((value & 0xF8) >> 3) + s->note));
    s->arpeggio_step++;
    const uint8_t head = bds_u8(m, entry);
    if (s->arpeggio_step == (head & 7) + 1) {
        s->arpeggio_step = (head & 0x80) != 0 ? 0 : ARPEGGIO_DONE;
    }
}

// Portamento: after a delay, the period moves by a step every tick for the given number of ticks.

static void run_portamento(BdsVoice* s) {
    if (s->portamento_delay != 0) {
        s->portamento_delay--;
        return;
    }
    if (s->portamento_ticks == 0) {
        return;
    }
    if (s->portamento_ticks != PORTAMENTO_FOREVER) {
        s->portamento_ticks--;
    }
    if (s->portamento_up) {
        s->period = (uint16_t)(s->period + s->portamento_step);
    } else {
        s->period = (uint16_t)(s->period - s->portamento_step);
        if (s->period < PORTAMENTO_MIN_PERIOD) {
            s->period = PORTAMENTO_MIN_PERIOD;
        }
    }
}

// What a voice does on a tick inside a note or a rest: the length counts down, and unless a voice starts a new event on
// this tick the envelope and the effects run.

static void run_note(BdsPlayer* p, BdsVoice* s) {
    if (s->rest_ticks != 0) {
        s->rest_ticks--;
        if (s->rest_ticks == 0) {
            s->note_ticks = 0;
        }
        return;
    }
    s->note_ticks--;
    if (p->note_ended) {
        return;
    }
    run_envelope(s);
    run_vibrato(p, s);
    run_arpeggio(p, s);
    run_portamento(s);
}

// The order list: a track number, or a command for the voice.

typedef enum BdsOrderCommand {
    BdsOrderCommand_Transpose = 0,
    BdsOrderCommand_Repeat = 1,
    BdsOrderCommand_Detune = 2,
    BdsOrderCommand_InstrumentMap = 3,
} BdsOrderCommand;

static BdsStep next_track(BdsPlayer* p, int voice) {
    BdsVoice* s = &p->voices[voice];
    BdsMemory* m = &p->module.memory;
    for (int steps = 0; steps < MAX_READER_STEPS; steps++) {
        const uint8_t entry = order_byte(p, s);
        s->order_pos++;
        if (entry == ORDER_LIST_END) {
            return BdsStep_SongEnd;
        }
        if ((entry & 0x80) == 0) {
            s->track = bds_u32(m, p->module.tables.tracks + 4 * (uint32_t)entry);
            s->track_pos = 0;
            return BdsStep_Playing;
        }
        const uint8_t argument = entry & 0x1F;
        switch ((BdsOrderCommand)((entry & 0x60) >> 5)) {
            case BdsOrderCommand_Transpose:
                s->transpose = order_byte(p, s);
                s->order_pos++;
                break;
            case BdsOrderCommand_Repeat:
                s->repeat = (uint8_t)(argument - 1);
                break;
            case BdsOrderCommand_Detune:
                s->detune = order_byte(p, s);
                s->order_pos++;
                break;
            case BdsOrderCommand_InstrumentMap:
                bds_put_u8(m, p->module.tables.instrument_map + (uint8_t)(MAP_VOICE_STRIDE * voice + argument),
                           order_byte(p, s));
                s->order_pos++;
                break;
        }
    }
    return BdsStep_SongEnd;
}

#define TRACK_END 0xFF
#define TRACK_REST 0xA0
#define TRACK_PORTAMENTO_OFF 0xBF

// The bytes of a track: notes are below 0x80, the rest are commands.
typedef enum BdsTrackCommand {
    BdsTrackCommand_Note = 0,
    BdsTrackCommand_Instrument, // 0x80..0x8F
    BdsTrackCommand_Arpeggio,   // 0x90..0xBE except the rest
    BdsTrackCommand_Rest,       // 0xA0, followed by the length
    BdsTrackCommand_PortamentoOff,
    BdsTrackCommand_Portamento, // 0xC0..0xFE and three bytes
    BdsTrackCommand_End,
} BdsTrackCommand;

static BdsTrackCommand track_command(uint8_t byte) {
    if (byte == TRACK_END) {
        return BdsTrackCommand_End;
    }
    if ((byte & 0x80) == 0) {
        return BdsTrackCommand_Note;
    }
    if (byte == TRACK_REST) {
        return BdsTrackCommand_Rest;
    }
    if (byte < 0x90) {
        return BdsTrackCommand_Instrument;
    }
    if (byte < 0xC0) {
        return byte == TRACK_PORTAMENTO_OFF ? BdsTrackCommand_PortamentoOff : BdsTrackCommand_Arpeggio;
    }
    return BdsTrackCommand_Portamento;
}

// A portamento command: its byte holds the direction and the delay, three bytes follow.

typedef struct BdsPortamento {
    bool up;
    uint8_t delay;
    uint16_t step;
    uint8_t ticks;
} BdsPortamento;

static BdsPortamento read_portamento(const BdsPlayer* p, const BdsVoice* s, uint8_t pos) {
    const uint8_t command = track_byte(p, s, pos);
    const uint8_t low = track_byte(p, s, (uint8_t)(pos + 1));
    const uint8_t high = track_byte(p, s, (uint8_t)(pos + 2));
    return (BdsPortamento) { .up = (command & 0x20) != 0,
                             .delay = command & 0x1F,
                             .step = (uint16_t)(high << 8 | low),
                             .ticks = track_byte(p, s, (uint8_t)(pos + 3)) };
}

static void start_portamento(const BdsPlayer* p, BdsVoice* s) {
    const BdsPortamento portamento = read_portamento(p, s, s->track_pos);
    s->portamento_up = portamento.up;
    s->portamento_delay_reload = portamento.delay;
    s->portamento_delay = portamento.delay;
    s->portamento_step = portamento.step;
    s->portamento_ticks = portamento.ticks;
    s->track_pos = (uint8_t)(s->track_pos + 3);
}

// A voice on a tick: inside a note it runs the envelope and effects, at the end of one it reads the track until it
// finds the next note or rest.

static BdsStep step_voice(BdsPlayer* p, int voice) {
    BdsVoice* s = &p->voices[voice];
    if (s->note_ticks != 0) {
        if (s->note_ticks == s->gate_ticks) {
            s->gate = false;
        }
        run_note(p, s);
        return BdsStep_Playing;
    }
    if (s->portamento_ticks != PORTAMENTO_FOREVER) {
        s->portamento_ticks = 0;
    }
    for (int steps = 0; steps < MAX_READER_STEPS; steps++) {
        const uint8_t byte = track_byte(p, s, s->track_pos);
        switch (track_command(byte)) {
            case BdsTrackCommand_End:
                if (s->repeat != 0) {
                    s->repeat--;
                    s->track_pos = 0;
                } else if (next_track(p, voice) == BdsStep_SongEnd) {
                    return BdsStep_SongEnd;
                }
                continue;
            case BdsTrackCommand_Note:
                start_note(p, voice, byte);
                return BdsStep_Playing;
            case BdsTrackCommand_Rest:
                // The voice is silent for the given ticks
                s->track_pos++;
                s->rest_ticks = track_byte(p, s, s->track_pos);
                s->note_ticks = s->rest_ticks;
                s->track_pos++;
                s->period = 0;
                run_note(p, s);
                return BdsStep_Playing;
            case BdsTrackCommand_Instrument:
                select_instrument(p, voice, byte);
                break;
            case BdsTrackCommand_PortamentoOff:
                s->portamento_ticks = 0;
                break;
            case BdsTrackCommand_Arpeggio:
                s->arpeggio = (uint8_t)((byte & 0xF) << 3);
                break;
            case BdsTrackCommand_Portamento:
                start_portamento(p, s);
                break;
        }
        s->track_pos++;
    }
    return BdsStep_SongEnd;
}

// Paula: the registers of the three voices, in the order of the driver

static void irq_cancel_request(BdsPlayer* p, int voice) {
    p->channels[voice].irq_request = false;
    p->channels[voice].sample_pass_active = false;
}

static void write_hardware(BdsPlayer* p) {
    const BdsTables* t = &p->module.tables;
    const bool late = is_late(p);
    for (int v = 0; v < BDS_VOICES; v++) {
        // A voice without a period is switched off
        if (p->voices[v].period == 0) {
            nuade_paula_channel_dma_off(&p->channels[v].paula);
        }
    }

    uint32_t lc[BDS_VOICES];
    for (int v = 0; v < BDS_VOICES; v++) {
        const BdsVoice* s = &p->voices[v];
        uint16_t len_words = s->len_words;
        lc[v] = s->lc;
        if (!late && !p->channels[v].plays_wave) {
            lc[v] = t->silence_lc;
            len_words = SILENCE_WORDS;
        }
        if (lc[v] != s->written_lc) {
            write_audio_block(p, &p->channels[v], lc[v], len_words);
        }
    }

    for (int v = 0; v < BDS_VOICES; v++) {
        BdsVoice* s = &p->voices[v];
        BdsChannel* c = &p->channels[v];
        if (late) {
            if (s->sample_state == BdsSampleState_Playing) {
                // A sample starts over: the channel stops and its interrupt request goes
                nuade_paula_channel_dma_off(&c->paula);
                irq_cancel_request(p, v);
            }
        } else if (s->written_lc == t->silence_lc && lc[v] != t->silence_lc) {
            nuade_paula_channel_dma_off(&c->paula);
        }
        s->written_lc = lc[v];
    }

    for (int v = 0; v < BDS_VOICES; v++) {
        p->channels[v].paula.period = p->voices[v].period;
    }

    // The voices that have a period and do not run are switched on
    for (int v = 0; v < BDS_VOICES; v++) {
        if (p->voices[v].period != 0 && !p->channels[v].paula.dma) {
            nuade_paula_channel_dma_on(&p->channels[v].paula);
        }
    }

    for (int v = 0; v < BDS_VOICES; v++) {
        p->channels[v].irq_enabled = true;
    }
    irq_poll_enabled(p);
}

// The late driver sets the volume of a sample instrument's voice here as well, from the same envelope as the interrupt
// but scaled by 4. The cap is scaled in voice 0's part, which only runs if voice 0 is a sample voice.

static void write_sample_volumes(BdsPlayer* p) {
    uint16_t cap = bds_u8(&p->module.memory, p->module.tables.volume_cap);
    for (int v = 0; v < BDS_VOICES; v++) {
        BdsVoice* s = &p->voices[v];
        if (s->sample_state == BdsSampleState_None) {
            continue;
        }
        const uint16_t volume = capped_volume(p, v, 2, &cap);
        if (volume != s->written_volume) {
            p->channels[v].paula.volume = (uint8_t)volume;
        }
        s->written_volume = volume;
    }
}

// Song start: all voices stop on an order list of their own and the instrument map takes its song start values.

static void reset_hardware(BdsPlayer* p) {
    for (int v = 0; v < BDS_VOICES; v++) {
        BdsChannel* c = &p->channels[v];
        nuade_paula_channel_dma_off(&c->paula);
        c->paula.period = 0;
        c->paula.len_words = IDLE_WORDS;
    }
}

static void init_song(BdsPlayer* p, int subsong) {
    const BdsTables* t = &p->module.tables;
    BdsMemory* m = &p->module.memory;
    memset(p->voices, 0, sizeof(p->voices));
    if (is_late(p)) {
        for (int v = 0; v < BDS_VOICES; v++) {
            p->voices[v].order_list = bds_u32(m, t->subsong_orders + 12 * (uint32_t)subsong + 4 * (uint32_t)v);
        }
    } else {
        for (int v = 0; v < BDS_VOICES; v++) {
            p->voices[v].order_list = t->order_lists[v];
        }
    }
    reset_hardware(p);
    for (int v = 0; v < BDS_VOICES; v++) {
        p->voices[v].wave_table = t->idle_wave_table;
        p->voices[v].track = t->default_track;
    }
    for (uint32_t i = 0; i < BDS_MAP_SLOTS; i++) {
        bds_put_u8(m, t->instrument_map + i, bds_u8(m, t->instrument_map_default + i));
    }
    p->song_active = true;
}

// The tick

static void play_tick(BdsPlayer* p) {
    const bool late = is_late(p);
    for (int v = 0; v < BDS_VOICES; v++) {
        if (p->voices[v].note_ticks == 1) {
            p->voices[v].note_ticks = 0;
            p->note_ended = true;
        }
    }
    for (int v = 0; v < BDS_VOICES; v++) {
        if (step_voice(p, v) == BdsStep_SongEnd) {
            if (late) {
                p->song_active = false;
            } else {
                // The early driver starts the song over on the spot
                init_song(p, p->subsong);
                p->looped = true;
            }
            return;
        }
    }
    if (!p->note_ended) {
        write_hardware(p);
    }
    p->note_ended = false;
    if (late) {
        write_sample_volumes(p);
    }
}

// The eagleplayer ends the sound when the late driver's order list ends, tells the player that the song has ended and
// starts the same subsong again.

static void restart_song(BdsPlayer* p) {
    for (int v = 0; v < BDS_VOICES; v++) {
        BdsChannel* c = &p->channels[v];
        nuade_paula_channel_dma_off(&c->paula);
        c->paula.volume = 0;
    }
    p->looped = true;
    init_song(p, p->subsong);
}

void bds_driver_tick(BdsPlayer* p) {
    p->looped = false;
    irq_replay_block_starts(p);
    play_tick(p);
    if (is_late(p) && !p->song_active) {
        restart_song(p);
    }
}

void bds_driver_start(BdsPlayer* p, int subsong) {
    BdsModule* module = &p->module;
    memcpy(module->memory.data, module->pristine, module->memory.size);
    memset(p->channels, 0, sizeof(p->channels));
    for (int v = 0; v < BDS_VOICES; v++) {
        p->channels[v].display_note = -1;
        p->channels[v].display_instrument = -1;
    }
    p->subsong = subsong;
    p->note_ended = false;
    p->looped = false;
    init_song(p, subsong);
    for (int v = 0; v < BDS_VOICES; v++) {
        // The mixer counts wraps from its start: only the ones after this point are block starts of the song
        NuadeVoiceState state;
        nuade_voice_get_state(p->mixer, (uint32_t)v, &state);
        p->channels[v].irq_wraps = state.wraps;
    }
}

// The first pass of a sample instrument plays as a one-shot: the driver's interrupt stops the channel when the pass is
// through, and a one-shot ends there instead of repeating until the next tick.

void bds_driver_sync(BdsPlayer* p) {
    for (int v = 0; v < BDS_VOICES; v++) {
        BdsChannel* c = &p->channels[v];
        const NuadePaulaChannel start = c->paula;
        const bool sample_start = start.dma_started && start.dma && start.start_lc != nullptr
                                  && p->voices[v].sample_state != BdsSampleState_None;
        nuade_paula_channel_sync(&c->paula, (uint32_t)v, p->mixer);
        if (sample_start) {
            const NuadeSample pass = { .data = start.start_lc,
                                       .format = NuadeSampleFormat_S8,
                                       .length = start.start_len_words != 0 ? (uint32_t)start.start_len_words * 2
                                                                            : NUADE_PAULA_MAX_SAMPLE_BYTES,
                                       .loop_start = 0,
                                       .loop_length = 0 };
            nuade_voice_play(p->mixer, (uint32_t)v, &pass, 0);
            c->sample_pass_active = true;
        }
    }
}
