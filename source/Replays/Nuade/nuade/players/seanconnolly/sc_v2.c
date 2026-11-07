// SPDX-License-Identifier: MIT
// Sean Connolly, sample replayer: the Surf Ninjas build, one tick per call.
//
// What the eagleplayer's glue does that matters here: it calls the module's interrupt routine once per 50 Hz tick, and
// before it, when all four voices have reached the end of their streams, SongEnd and the init routine again. Its volume
// patch masks the volume with 0x7F and scales by 64/64.
//
// A tick: the registers staged by the previous tick are written (DMA of the voices that start a sample off first and on
// afterwards), then every voice runs, voice 3 first, and stages its registers for the next tick. The module is a
// memory image of the player: its variables hold the player's state at the moment the composer saved it, so the
// register blocks and the noise seed of the first tick come from the file.

#include "sc_v2.h"

#include "nuade_base.h"

#include <string.h>

#define V2_BUDGET 0x10000u // iterations of a stream or pattern loop before the port gives up
#define V2_SVX 0x38535658u // '8SVX' at +8 of a sample
#define V2_SVX_AT 8
#define V2_SVX_ONESHOT 0x16 // the low words of the one-shot and repeat lengths
#define V2_SVX_REPEAT 0x1A
#define V2_SVX_BODY 0x68
#define V2_INSTRUMENT_SHIFT 4
#define V2_MAX_VOLUME 0x40
#define V2_SPEED_INITIAL 6
#define V2_SPEED_COUNTER_INITIAL 4
#define V2_NOISE_POS 0xFFFu
#define V2_NOISE_HALF_BYTES 0x40 // the second noise buffer follows the first
#define V2_WAVE_WORDS 8
#define V2_NOTE_END 0xFF

// Addresses of the replayer's variables, from the init entry (in the Surf Ninjas module: init at 0x3AC).
#define V2_STAGE_AT (-0x11A)        // 0x292, four blocks of 16 bytes
#define V2_WRITTEN_AT (-0xDA)       // 0x2D2
#define V2_NOISE_TOGGLE_AT (-0x148) // 0x264
#define V2_REGISTER_BLOCK 16

typedef enum StreamCommand {
    StreamCommand_End = 0xFE,
    StreamCommand_Fade = 0xFD,
} StreamCommand;

typedef enum PatternCommand {
    PatternCommand_Slide = 0xFE,
    PatternCommand_ArpTable = 0xFD,
    PatternCommand_Rest = 0xFC,
    PatternCommand_ChunkSize = 0xFB,
    PatternCommand_NoRetriggerOn = 0xFA,
    PatternCommand_NoRetriggerOff = 0xF9,
    PatternCommand_SequenceTranspose = 0xF8,
} PatternCommand;

// The arpeggio table's control bytes.
typedef enum ArpByte {
    ArpByte_End = 0xFE,  // the next byte is the voice's new effect
    ArpByte_Jump = 0xFF, // the next byte is the new position
} ArpByte;

// The low nibble of an instrument's mode byte (any other value: an 8SVX sample).
typedef enum WaveType {
    WaveType_Sweep = 0,
    WaveType_Chunks = 2,
    WaveType_Sequence = 3,
} WaveType;

typedef enum Effect {
    Effect_Vibrato = 1,
    Effect_Arpeggio = 2,
} Effect;

// Instrument record fields (16 bytes).
typedef enum InstrumentField {
    InstrumentField_Mode = 0,
    InstrumentField_Volume = 1,
    InstrumentField_Target = 2,
    InstrumentField_End = 3,
    InstrumentField_AttackRelease = 4,
    InstrumentField_Periods = 5,
    InstrumentField_Vibrato = 6,
    InstrumentField_Effect = 7,
    InstrumentField_SweepPos = 8,
    InstrumentField_SweepLower = 10,
    InstrumentField_SweepUpper = 12,
    InstrumentField_SweepStep = 14,
    InstrumentField_Sample = 15,
} InstrumentField;

typedef struct Instrument {
    uint8_t mode;
    uint8_t volume;
    uint8_t target;
    uint8_t end;
    uint8_t attack_step;
    uint8_t release_step;
    uint8_t attack_every;
    uint8_t release_every;
    uint8_t vibrato_delay;
    uint8_t vibrato_shift;
    uint8_t effect;
    uint16_t sweep_pos;
    uint16_t sweep_lower;
    uint16_t sweep_upper;
    uint8_t sweep_step;
    uint8_t sample;
} Instrument;

static uint32_t var_address(const ScoModule* m, int32_t at) {
    return (uint32_t)((int32_t)m->entry + at);
}

// An instrument record. `index` is the instrument number; the record is 16 bytes behind the number shifted left.

static Instrument instrument_read(const ScoModule* m, uint32_t index) {
    const uint32_t at = m->base[ScoRole_Instruments] + (uint32_t)(uint16_t)(index << V2_INSTRUMENT_SHIFT);
    const uint8_t attack_release = sco_u8(m, at + InstrumentField_AttackRelease);
    const uint8_t periods = sco_u8(m, at + InstrumentField_Periods);
    const uint8_t vibrato = sco_u8(m, at + InstrumentField_Vibrato);
    return (Instrument) {
        .mode = sco_u8(m, at + InstrumentField_Mode),
        .volume = sco_u8(m, at + InstrumentField_Volume),
        .target = sco_u8(m, at + InstrumentField_Target),
        .end = sco_u8(m, at + InstrumentField_End),
        .attack_step = (uint8_t)(attack_release >> 4),
        .release_step = (uint8_t)(attack_release & 0xF),
        .attack_every = (uint8_t)(periods >> 4),
        .release_every = (uint8_t)(periods & 0xF),
        .vibrato_delay = (uint8_t)((vibrato & 0xF0) >> 2),
        .vibrato_shift = (uint8_t)(vibrato & 7),
        .effect = sco_u8(m, at + InstrumentField_Effect),
        .sweep_pos = sco_u16(m, at + InstrumentField_SweepPos),
        .sweep_lower = sco_u16(m, at + InstrumentField_SweepLower),
        .sweep_upper = sco_u16(m, at + InstrumentField_SweepUpper),
        .sweep_step = sco_u8(m, at + InstrumentField_SweepStep),
        .sample = sco_u8(m, at + InstrumentField_Sample),
    };
}

// The period table is indexed by a word: the note doubled.

static uint16_t period_at(const ScoModule* m, uint16_t index) {
    return sco_u16_indexed(m, m->base[ScoRole_Period], index);
}

static uint16_t note_period(const ScoModule* m, uint8_t note) {
    return period_at(m, (uint16_t)(note * 2));
}

// An 8SVX sample as the replayer reads its header: the chunk id at +8, the low words of the one-shot and repeat lengths
// and the body at a fixed offset.

typedef struct Svx {
    bool valid;
    uint16_t oneshot;
    uint16_t repeat;
    uint32_t body;
} Svx;

static Svx svx_read(const ScoModule* m, uint32_t start) {
    return (Svx) { .valid = sco_u32(m, start + V2_SVX_AT) == V2_SVX,
                   .oneshot = sco_u16(m, start + V2_SVX_ONESHOT),
                   .repeat = sco_u16(m, start + V2_SVX_REPEAT),
                   .body = start + V2_SVX_BODY };
}

// A sample of the sample table (the entries are 4 bytes; the index is scaled in a word).

static ScoSampleBounds sample_bounds(const ScoModule* m, uint8_t index) {
    return sco_sample_bounds(m, (uint16_t)(index << 2));
}

// The noise generator: 32 words into one of the two buffers, alternately. The seed (x, y, z) lives in the image.

static void noise_generate(Sco2* s) {
    ScoModule* m = s->module;
    const uint32_t seed = sco_seed_address(m);
    const uint32_t first = sco_noise_address(m);
    s->noise_toggle = (uint16_t)~s->noise_toggle;
    uint32_t buffer = first;
    if ((s->noise_toggle & 0x8000) != 0) {
        buffer += V2_NOISE_HALF_BYTES;
    }
    for (int32_t offset = 0x3E; offset >= 0; offset -= 2) {
        const uint16_t x = sco_u16(m, seed);
        const uint16_t y = sco_u16(m, seed + 2);
        const uint16_t z = sco_u16(m, seed + 4);
        const uint16_t value = (uint16_t)(x + x + z + x + y);
        sco_write16(m, seed, value);
        sco_write16(m, seed + 2, (uint16_t)(y + 1));
        sco_write16(m, seed + 4, (uint16_t)(z + 2));
        sco_write16(m, buffer + (uint32_t)offset, value);
    }
}

// Stages the registers of a wave form of the 8-byte wave table, or the noise buffer.

static void stage_wave(const ScoModule* m, Sco2Voice* v, uint32_t offset) {
    v->staged.lc = m->base[ScoRole_Wave8] + offset;
    v->staged.len_words = V2_WAVE_WORDS;
}

static void stage_noise(const ScoModule* m, Sco2Voice* v) {
    v->staged.lc = sco_noise_address(m);
    v->staged.len_words = m->version->noise_words;
}

// The volume of a voice: the attack, then the release; held under the master level.

static void envelope_stage_volume(Sco2Voice* v, uint8_t volume) {
    if ((int8_t)v->envelope.limit < (int8_t)volume) {
        volume = v->envelope.limit;
    }
    v->staged.volume = volume;
}

static void envelope_step(Sco2Voice* v) {
    Sco2Envelope* e = &v->envelope;
    if (!e->releasing) {
        if (e->attack_count == e->attack_every) {
            e->attack_count++;
            return;
        }
        e->attack_count = 0;
        if (e->target == e->volume) {
            e->releasing = true;
            return;
        }
        e->volume = (uint8_t)(e->volume + e->attack_step);
        envelope_stage_volume(v, e->volume);
        return;
    }
    if (e->release_count != e->release_every) {
        e->release_count++;
        return;
    }
    e->release_count = 0;
    if (e->end != e->target) {
        const int8_t lowered = (int8_t)(e->target - e->release_step);
        e->target = lowered < 0 ? 0 : (uint8_t)lowered;
    }
    envelope_stage_volume(v, e->target);
}

// The master fade out of the volume limit.

static void fade_step(Sco2Voice* v) {
    Sco2Envelope* e = &v->envelope;
    if (!e->fading) {
        return;
    }
    if (e->fade_every != e->fade_count) {
        e->fade_count++;
        return;
    }
    e->fade_count = 0;
    if (e->limit != 0) {
        e->limit--;
    }
}

// A sample that loops: its repeat part, or the end of the sample.

static void sample_loop(const ScoModule* m, Sco2Voice* v) {
    const ScoSampleBounds bounds = sample_bounds(m, v->sample);
    const Svx svx = svx_read(m, bounds.start);
    if (svx.valid && (int16_t)svx.repeat > 4) {
        v->staged.lc = svx.body + svx.oneshot;
        v->staged.len_words = (uint16_t)(svx.repeat >> 1);
        return;
    }
    v->staged.lc = bounds.end - 4;
    v->staged.len_words = 1;
}

// A swept wave form: the position moves by the step and turns at the limits.

static void sweep_step(const ScoModule* m, Sco2Voice* v) {
    if (!v->sweep_down) {
        if ((int16_t)v->sweep_upper <= (int16_t)v->sweep_pos) {
            v->sweep_down = !v->sweep_down;
            return;
        }
        v->sweep_pos = (uint16_t)(v->sweep_pos + v->sweep_step);
    } else {
        if ((int16_t)v->sweep_lower >= (int16_t)v->sweep_pos) {
            v->sweep_down = !v->sweep_down;
            return;
        }
        v->sweep_pos = (uint16_t)(v->sweep_pos - v->sweep_step);
    }
    if (v->sweep_pos == V2_NOISE_POS) {
        stage_noise(m, v);
        return;
    }
    stage_wave(m, v, (uint32_t)v->sweep_pos << 4);
}

// A sample played in chunks of the chunk size, one more each tick; at the end the voice goes silent and sweeps.

static void chunk_step(const ScoModule* m, Sco2Voice* v) {
    const ScoSampleBounds bounds = sample_bounds(m, v->sample);
    const Svx svx = svx_read(m, bounds.start);
    const uint32_t body = svx.valid ? svx.body : bounds.start;
    uint32_t remaining = bounds.end - body;
    const uint32_t size = v->chunk_bytes;
    const uint32_t offset = (uint32_t)(uint16_t)size * v->chunk_count;
    remaining -= size;
    if ((remaining & 0xFFFF) <= (offset & 0xFFFF)) {
        v->envelope.volume = 0;
        v->envelope.target = 0;
        v->envelope.end = 0;
        v->staged.volume = 0;
        v->mode = (uint8_t)(v->mode & 0xF0);
        return;
    }
    v->staged.lc = body + offset;
    v->staged.len_words = (uint16_t)(size >> 1);
    v->chunk_count++;
}

// A sequence of wave forms: records of 32 words (the wave in the first half, the note in the second); a wave of 0xFFFF
// jumps to the record in the next word. Switching from the noise wave restarts the voice's DMA.

static void sequence_step(Sco2* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    const uint32_t records = m->base[ScoRole_WaveSequences] + ((uint32_t)(uint16_t)(v->sample << 6));
    for (uint32_t guard = 0; guard < V2_BUDGET; guard++) {
        v->sequence_before = v->sequence_wave;
        const uint16_t at = v->sequence_pos;
        const uint16_t wave = sco_u16(m, records + at);
        uint16_t note = sco_u16(m, records + at + 0x20);
        if (wave == 0xFFFF) {
            v->sequence_pos = (uint16_t)(sco_u16(m, records + at + 2) * 2);
            continue;
        }
        v->sequence_wave = wave;
        v->sweep_pos = wave;
        if (wave == V2_NOISE_POS) {
            stage_noise(m, v);
        } else {
            stage_wave(m, v, (uint32_t)(uint16_t)(wave << 4));
            if (v->sequence_before == V2_NOISE_POS) {
                s->restart_next[index] = true;
            }
        }
        note = (uint16_t)((note & 0xFF00) | (uint8_t)(note + v->sequence_transpose));
        v->staged.period = period_at(m, (uint16_t)(note * 2));
        v->sequence_pos = (uint16_t)((v->sequence_pos + 2) & 0x1F);
        return;
    }
    s->unmodeled = true;
}

// A pitch slide to the voice's note, from the note the pattern gave: the period moves by the rate each tick.

static void slide_step(const ScoModule* m, Sco2Voice* v) {
    const uint16_t target = note_period(m, v->note);
    if (v->slide_start < v->note) {
        v->period = (uint16_t)(v->period - v->slide_rate);
        v->staged.period = v->period;
        if ((int16_t)target < (int16_t)v->period) {
            return;
        }
    } else {
        v->period = (uint16_t)(v->period + v->slide_rate);
        v->staged.period = v->period;
        if ((int16_t)target > (int16_t)v->period) {
            return;
        }
    }
    v->sliding = false;
    v->staged.period = v->note_period;
    v->period = v->note_period;
}

// Vibrato: after the delay the period swings by the step (the distance to the next note, shifted) and half of it, for
// two cycles' worth of ticks.

static void vibrato_step(const ScoModule* m, Sco2Voice* v) {
    if (v->vibrato_delay != 0) {
        v->vibrato_delay--;
        return;
    }
    v->vibrato_step = (uint16_t)(sco_u16_indexed(m, m->base[ScoRole_Period] - 2, (uint16_t)(v->note * 2))
                                 - note_period(m, v->note));
    const uint8_t first = (uint8_t)(v->cycle + v->cycle);
    const uint8_t second = (uint8_t)(first + first);
    if ((int8_t)v->phase >= (int8_t)second && (int8_t)v->phase >= (int8_t)first) {
        v->period = note_period(m, v->note);
        v->phase = 0;
        v->staged.period = v->period;
        return;
    }
    uint16_t step = (uint16_t)(v->vibrato_step >> v->vibrato_shift);
    if ((int8_t)v->phase < (int8_t)first) {
        v->period = (uint16_t)(v->period + step);
        step = (uint16_t)(step >> 1);
        v->period = (uint16_t)(v->period - step);
    } else {
        v->period = (uint16_t)(v->period - step);
        step = (uint16_t)(step >> 1);
        v->period = (uint16_t)(v->period + step);
    }
    v->staged.period = v->period;
    v->phase++;
}

// Arpeggio: after the delay, the next entry of the voice's arpeggio table is the period. Entries below 0x19 are
// relative to the note, 0xFF jumps to the position in the next byte and 0xFE ends the arpeggio by switching the
// voice's effect to the next byte.

static void arpeggio_step(Sco2* s, Sco2Voice* v) {
    const ScoModule* m = s->module;
    if (v->phase != 0) {
        v->phase--;
        return;
    }
    v->phase = v->cycle;
    const uint32_t table
        = m->base[ScoRole_SeqBase] + sco_u32_indexed(m, m->base[ScoRole_SeqTable], (uint16_t)(v->arp_table << 2));
    uint8_t pos = v->arp_pos;
    for (uint32_t guard = 0; guard < V2_BUDGET; guard++) {
        const uint8_t entry = sco_u8(m, table + pos);
        if (entry == ArpByte_Jump) {
            pos = sco_u8(m, table + pos + 1);
            v->arp_pos = pos;
            continue;
        }
        if (entry == ArpByte_End) {
            v->effect = sco_u8(m, table + pos + 1);
            return;
        }
        uint16_t note = entry;
        if ((int8_t)entry < 0x19) {
            note = (uint8_t)(v->note + entry);
        }
        v->staged.period = period_at(m, (uint16_t)(note * 2));
        v->arp_pos++;
        return;
    }
    s->unmodeled = true;
}

// The wave type's per-tick work, then the pitch: a slide, or the voice's effect.

static void wave_step(Sco2* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    switch (v->mode & 0xF) {
        case WaveType_Chunks:
            chunk_step(m, v);
            break;
        case WaveType_Sequence:
            sequence_step(s, index);
            return;
        case WaveType_Sweep:
            sweep_step(m, v);
            break;
        default:
            sample_loop(m, v);
            break;
    }
    if (v->sliding) {
        slide_step(m, v);
        return;
    }
    if (v->effect == Effect_Arpeggio) {
        arpeggio_step(s, v);
    } else if (v->effect == Effect_Vibrato) {
        vibrato_step(m, v);
    }
}

// What the voice does between events: the instrument's parameters are read again every tick, a new note starts its
// envelope and effect, then the master fade, the envelope and the wave type.

static void effects_step(Sco2* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    const Instrument in = instrument_read(m, v->instrument & 0x7F);
    Sco2Envelope* e = &v->envelope;
    v->sweep_lower = in.sweep_lower;
    v->sweep_upper = in.sweep_upper;
    v->sweep_step = in.sweep_step;
    e->end = in.end;
    e->attack_step = in.attack_step;
    e->release_step = in.release_step;
    e->attack_every = in.attack_every;
    e->release_every = in.release_every;
    v->cycle = (uint8_t)(in.mode >> 4);
    if (v->new_note) {
        const Instrument start = instrument_read(m, v->instrument);
        v->new_note = false;
        v->arp_pos = 0;
        v->sweep_down = false;
        e->target = start.target;
        e->attack_count = 0;
        e->release_count = 0;
        v->effect = start.effect;
        v->vibrato_delay = start.vibrato_delay;
        v->phase = 0;
        v->vibrato_shift = start.vibrato_shift;
    }
    fade_step(v);
    envelope_step(v);
    wave_step(s, index);
}

// The registers for a note, by wave type. The voice's DMA restarts afterwards.

static void note_wave(Sco2* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    switch (v->mode & 0xF) {
        case WaveType_Chunks: {
            // the replayer scales the sample number in a byte register: samples from 64 on wrap
            const ScoSampleBounds bounds = sco_sample_bounds(m, (uint8_t)(v->sample * 4));
            v->staged.lc = svx_read(m, bounds.start).body;
            v->staged.len_words = (uint16_t)(v->chunk_bytes >> 1);
            break;
        }
        case WaveType_Sequence: {
            const uint32_t records = m->base[ScoRole_WaveSequences] + (uint32_t)(uint16_t)(v->sample << 6);
            const uint16_t wave = sco_u16(m, records);
            const uint16_t note = sco_u16(m, records + 0x20);
            if (wave == V2_NOISE_POS) {
                stage_noise(m, v);
            } else {
                stage_wave(m, v, (uint32_t)(uint16_t)(wave << 4));
            }
            v->staged.period = period_at(m, (uint16_t)(note * 2));
            v->period = v->staged.period;
            v->sequence_pos = 2;
            v->sequence_wave = wave;
            v->sequence_before = wave;
            break;
        }
        case WaveType_Sweep:
            if (v->sweep_pos == V2_NOISE_POS) {
                stage_noise(m, v);
            } else {
                v->staged.lc = m->base[ScoRole_Wave8] + (uint32_t)(int32_t)(int16_t)(uint16_t)(v->sweep_pos << 4);
                v->staged.len_words = V2_WAVE_WORDS;
            }
            break;
        default: {
            const ScoSampleBounds bounds = sample_bounds(m, v->sample);
            const Svx svx = svx_read(m, bounds.start);
            v->staged.lc = svx.body;
            if (svx.valid) {
                v->staged.len_words = (uint16_t)(((uint32_t)svx.oneshot + svx.repeat) >> 1);
            } else {
                v->staged.len_words = (uint16_t)((bounds.end - 2 - svx.body) >> 1);
            }
            break;
        }
    }
    s->restart_next[index] = true;
}

// A note: the period (a slide keeps the period it started from), the instrument's first registers, the wave.

static void note_start(Sco2* s, uint32_t index, uint8_t byte) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    v->new_note = true;
    v->envelope.releasing = false;
    v->chunk_count = 0;
    v->note = (uint8_t)(byte + v->transpose);
    v->note_period = period_at(m, (uint16_t)(uint8_t)(v->note * 2));
    if (!v->sliding) {
        v->period = v->note_period;
    }
    v->length_counter = v->length;
    v->played = true;
    const Instrument in = instrument_read(m, v->instrument);
    v->mode = in.mode;
    v->envelope.volume = in.volume;
    v->sweep_pos = in.sweep_pos;
    v->sweep_down = false;
    v->sample = in.sample;
    v->staged.period = v->period;
    uint8_t volume = in.volume;
    if (v->envelope.fading && (int8_t)v->envelope.limit < (int8_t)volume) {
        volume = v->envelope.limit;
    }
    v->staged.volume = volume;
    if (v->no_retrigger) {
        return;
    }
    note_wave(s, index);
}

// The end of an event: a note held for further plays does not move on, else the pattern position advances; at the end
// of the pattern it repeats or the stream goes on (a stream byte 0xFF behind the pattern number jumps back).

static void event_end(Sco2* s, uint32_t index, uint32_t pattern) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    if (v->hold != 0) {
        v->hold--;
        return;
    }
    v->pattern_pos++;
    if (sco_u8(m, pattern + v->pattern_pos) != V2_NOTE_END) {
        return;
    }
    v->pattern_pos = 0;
    if (v->pattern_repeat != 0) {
        v->pattern_repeat--;
        return;
    }
    v->stream_pos++;
    if (s->lookahead[1] != 0xFF) {
        return;
    }
    v->stream_pos = s->lookahead[2];
    // the eagleplayer patched this jump of the module to call SongEnd when voice 0 takes it
    if (index == 0) {
        s->song_end = true;
    }
    v->transpose = 0;
    v->instrument_offset = 0;
    v->envelope.fading = false;
    v->envelope.fade_count = 0;
    v->stream_restart = true;
}

// Reads the pattern from the voice's position until an event is over: a note, or a rest.

static void pattern_run(Sco2* s, uint32_t index, uint32_t pattern) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    for (uint32_t guard = 0; guard < V2_BUDGET; guard++) {
        const uint8_t pos = v->pattern_pos;
        const uint8_t byte = sco_u8(m, pattern + pos);
        // operands are read at `$1(a0,d4.w)`: the displacement is added to the byte position without wrapping
        const uint8_t next = sco_u8(m, pattern + pos + 1);
        const uint8_t after = sco_u8(m, pattern + pos + 2);
        uint8_t advance = 1;
        switch (byte) {
            case PatternCommand_Slide: {
                v->sliding = true;
                const uint8_t start = (uint8_t)(next + v->transpose);
                v->slide_start = start;
                v->period = period_at(m, (uint16_t)(start * 2));
                v->slide_rate = after;
                v->pattern_pos = (uint8_t)(pos + 3);
                note_start(s, index, sco_u8(m, pattern + v->pattern_pos));
                event_end(s, index, pattern);
                return;
            }
            case PatternCommand_ArpTable:
                v->arp_table = next;
                advance = 2;
                break;
            case PatternCommand_Rest:
                v->length_counter = v->length;
                event_end(s, index, pattern);
                return;
            case PatternCommand_ChunkSize:
                v->chunk_bytes = (uint16_t)((next << 8) | after);
                advance = 3;
                break;
            case PatternCommand_NoRetriggerOn:
            case PatternCommand_NoRetriggerOff:
                v->no_retrigger = (byte & 1) == 0;
                break;
            case PatternCommand_SequenceTranspose:
                v->sequence_transpose = next;
                advance = 2;
                break;
            default:
                if ((byte & 0xC0) == 0xC0) {
                    v->instrument = (uint8_t)((byte & 0x3F) + v->instrument_offset);
                } else if ((byte & 0x80) != 0) {
                    v->length = (uint8_t)(byte - 0x7F);
                } else if ((byte & 0x70) == 0x70 || (byte & 0x70) == 0x60) {
                    v->hold = (uint8_t)((byte - 0x5D) & 0x3F);
                } else {
                    note_start(s, index, byte);
                    event_end(s, index, pattern);
                    return;
                }
                break;
        }
        v->pattern_pos = (uint8_t)(pos + advance);
    }
    s->unmodeled = true;
}

// The note counters ran out: read the stream. Modifier bytes change the voice and read on; a pattern number starts the
// pattern at the voice's position.

static void sequence_advance(Sco2* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco2Voice* v = &s->voice[index];
    v->sliding = false;
    if (v->stream_restart) {
        v->stream_restart = false;
        v->envelope.fading = false;
        v->envelope.limit = V2_MAX_VOLUME;
    }
    const uint32_t stream = v->stream_base;
    uint8_t pos = v->stream_pos;
    uint8_t byte = sco_u8(m, stream + pos);
    if (byte == StreamCommand_End) {
        v->active = false;
        return;
    }
    if (byte == StreamCommand_Fade) {
        for (uint32_t i = 0; i < SCO_VOICES; i++) {
            Sco2Envelope* e = &s->voice[i].envelope;
            e->fading = true;
            e->limit = V2_MAX_VOLUME;
            e->fade_count = 0;
            e->fade_every = sco_u8(m, stream + pos + 1);
        }
        pos = (uint8_t)(pos + 2);
        v->stream_pos = pos;
        byte = sco_u8(m, stream + pos);
    }
    if ((byte & 0xF0) == 0xF0) {
        v->speed = byte & 0xF;
        v->speed_counter = byte & 0xF;
        v->stream_pos = ++pos;
        byte = sco_u8(m, stream + pos);
    }
    for (uint32_t guard = 0; guard < V2_BUDGET; guard++) {
        if ((byte & 0xC0) == 0xC0) {
            v->transpose = byte & 0x3F;
            v->stream_pos = ++pos;
            byte = sco_u8(m, stream + pos);
        }
        if ((byte & 0x80) == 0x80) {
            v->pattern_repeat = byte & 0x3F;
            v->stream_pos = ++pos;
            byte = sco_u8(m, stream + pos);
        }
        if ((byte & 0x70) == 0x70 || (byte & 0x60) == 0x60) {
            v->instrument_offset = byte & 0x1F;
            v->stream_pos = ++pos;
            byte = sco_u8(m, stream + pos);
            continue;
        }
        const uint32_t number = (uint32_t)(byte & 0x7F) * 4;
        s->lookahead[0] = byte;
        s->lookahead[1] = sco_u8(m, stream + pos + 1);
        s->lookahead[2] = sco_u8(m, stream + pos + 2);
        const uint32_t pattern = m->base[ScoRole_PatternData] + sco_u32(m, m->base[ScoRole_PatternTable] + number);
        pattern_run(s, index, pattern);
        return;
    }
    s->unmodeled = true;
}

// One voice of a tick.

static void voice_run(Sco2* s, uint32_t index, NuadePaulaChannel* paula) {
    Sco2Voice* v = &s->voice[index];
    if (!v->active) {
        v->staged.period = 0;
        v->staged.volume = 0;
        nuade_paula_channel_dma_off(&paula[index]);
        return;
    }
    v->speed_counter--;
    if (v->speed_counter == 0) {
        v->speed_counter = v->speed;
        v->length_counter--;
        if (v->length_counter == 0) {
            sequence_advance(s, index);
            return;
        }
    }
    effects_step(s, index);
}

void sc2_tick(Sco2* s, NuadePaulaChannel* paula) {
    s->song_end = false;
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->restart[i]) {
            nuade_paula_channel_dma_off(&paula[i]);
        }
        s->restart_next[i] = false;
    }
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        sco_registers_write(s->module, &s->voice[i].written, &paula[i]);
    }
    for (int32_t i = SCO_VOICES - 1; i >= 0; i--) {
        voice_run(s, (uint32_t)i, paula);
    }
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->voice[i].sweep_pos == V2_NOISE_POS) {
            noise_generate(s);
            break;
        }
    }
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->restart[i]) {
            nuade_paula_channel_dma_on(&paula[i]);
        }
        s->restart[i] = s->restart_next[i];
        s->voice[i].written = s->voice[i].staged;
    }
}

bool sc2_ended(const Sco2* s) {
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->voice[i].active) {
            return false;
        }
    }
    return true;
}

// The registers of a block of the module's variables.

static ScoRegisters registers_read(const ScoModule* m, uint32_t at) {
    return (ScoRegisters) {
        .lc = sco_peek32(m, at),
        .len_words = sco_peek16(m, at + 4),
        .period = sco_peek16(m, at + 6),
        .volume = (uint8_t)sco_peek16(m, at + 8),
    };
}

// The init routine: two noise buffers are made, the variables cleared and set, DMA is off and the period and volume
// registers are 0. The register blocks and the noise seed's x and y are not touched; z is the raster position.

void sc2_init(Sco2* s, ScoModule* module, uint32_t subsong, bool first, NuadePaulaChannel* paula) {
    Sco2Voice previous[SCO_VOICES];
    memcpy(previous, s->voice, sizeof(previous));
    uint16_t toggle = s->noise_toggle;
    memset(s, 0, sizeof(*s));
    s->module = module;
    const uint32_t song = module->base[ScoRole_Songs] + subsong * SCO_SONG_BYTES;
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        Sco2Voice* v = &s->voice[i];
        if (first) {
            v->staged = registers_read(module, var_address(module, V2_STAGE_AT) + i * V2_REGISTER_BLOCK);
            v->written = registers_read(module, var_address(module, V2_WRITTEN_AT) + i * V2_REGISTER_BLOCK);
        } else {
            v->staged = previous[i].staged;
            v->written = previous[i].written;
        }
        v->stream_base = module->base[ScoRole_Streams] + sco_u32(module, song + i * 4);
    }
    if (first) {
        toggle = sco_peek16(module, var_address(module, V2_NOISE_TOGGLE_AT));
    }
    s->noise_toggle = toggle;
    sco_write16(module, sco_seed_address(module) + 4, SCO_V2_VHPOS);
    noise_generate(s);
    noise_generate(s);
    s->noise_toggle = 0;
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        Sco2Voice* v = &s->voice[i];
        v->active = true;
        v->speed = V2_SPEED_INITIAL;
        v->speed_counter = V2_SPEED_COUNTER_INITIAL;
        v->length = 1;
        v->length_counter = 1;
        v->envelope.limit = V2_MAX_VOLUME;
        nuade_paula_channel_dma_off(&paula[i]);
        paula[i].period = 0;
        paula[i].volume = 0;
    }
}
