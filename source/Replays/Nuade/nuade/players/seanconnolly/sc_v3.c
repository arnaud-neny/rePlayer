// SPDX-License-Identifier: MIT
// Sean Connolly, sequencer replayer: the Reaxion and Soccer Superstars builds, one tick per call.
//
// What the eagleplayer's glue does that matters here: it calls the module's interrupt routine once per 50 Hz tick, and
// before it, when all four voices have reached the end of their streams, SongEnd and the init routine again. Its volume
// patch masks the volume with 0x7F and scales by 64/64. The sound effect routines of the module (start, stop, the
// per-voice effect state they feed) are never called by the glue and are not part of the port.
//
// A tick: the registers staged by the previous tick are written (DMA of the voices that start a sample off first and on
// afterwards), then every voice runs, voice 3 first, and stages its registers for the next tick.

#include "sc_v3.h"

#include "nuade_base.h"

#include <stdlib.h>
#include <string.h>

#define V3_RESTART_BUDGET 0x10000u // iterations of a stream, pattern or wave sequence loop before the port gives up
#define V3_VHDR 0x56484452u        // 'VHDR' at +12 of an 8SVX sample
#define V3_VHDR_AT 0xC
#define V3_VHDR_ONESHOT 0x14
#define V3_VHDR_REPEAT 0x18
#define V3_VHDR_BODY 0x68
#define V3_NOTE_REST 0x5F
#define V3_NOTE_LENGTH_BASE 0x60
#define V3_NOTE_LENGTH_BIAS 0x5F
#define V3_PATTERN_END 0xFF
#define V3_MAX_VOLUME 0x40
#define V3_SPEED_INITIAL 6
#define V3_WAVE_JUMP 0xFFFFu
#define V3_NOISE_POS 0xFFFu
#define V3_INSTRUMENT_SHIFT 4
#define V3_SAMPLE_LOOP_MIN 4

// The command byte of a stream word (the high byte); the low byte is its argument.
typedef enum StreamCommand {
    StreamCommand_Jump = 0xFF, // with argument 0: continue at the word index in the next word
    StreamCommand_End = 0xFE,  // with argument 0
    StreamCommand_Speed = 0xFD,
    StreamCommand_Transpose = 0xFC,
    StreamCommand_InstrumentOffset = 0xFB,
    StreamCommand_MasterVolume = 0xFA,
    StreamCommand_Marker = 0xF9, // with argument 0
    StreamCommand_PatternRepeat = 0xF8,
    StreamCommand_LoopStart = 0xF7,
    StreamCommand_LoopEnd = 0xF6, // with argument 0
} StreamCommand;

// The pattern bytes with a command meaning.
typedef enum PatternCommand {
    PatternCommand_Slide = 0xFE,
    PatternCommand_Instrument = 0xFD,
    PatternCommand_PitchSequence = 0xFC,
    PatternCommand_SampleOffset = 0xFB,
    PatternCommand_Envelope = 0xFA,
    PatternCommand_Adkcon = 0xF9,
    PatternCommand_StreamChunk = 0xF8,
    PatternCommand_Marker = 0xF7,
    PatternCommand_NoRetriggerOff = 0xF5,
    PatternCommand_NoRetriggerOn = 0xF6,
} PatternCommand;

// Instrument record fields (16 bytes).
typedef enum InstrumentField {
    InstrumentField_Wave = 0,
    InstrumentField_Sample = 1,
    InstrumentField_PitchMode = 2,
    InstrumentField_Vibrato = 3,
    InstrumentField_VibratoDelay = 4,
    InstrumentField_Volume = 5,
    InstrumentField_VolumeTarget = 6,
    InstrumentField_VolumeSteps = 7,
    InstrumentField_VolumeDelay = 8,
    InstrumentField_SweepPos = 9,
    InstrumentField_SweepLower = 11,
    InstrumentField_SweepUpper = 13,
    InstrumentField_SweepStep = 15,
} InstrumentField;

// The voice's pitch effect (any other value selects a pitch sequence).
typedef enum PitchMode {
    PitchMode_None = 0,
    PitchMode_Vibrato = 1,
} PitchMode;

typedef struct Instrument {
    uint8_t wave;
    uint8_t sample;
    uint8_t pitch_mode;
    uint8_t vibrato_depth;
    uint8_t vibrato_shift;
    uint8_t vibrato_delay;
    uint8_t volume;
    uint8_t volume_target;
    uint8_t volume_step;
    uint8_t volume_interval;
    uint8_t volume_delay;
    uint16_t sweep_pos;
    uint16_t sweep_lower;
    uint16_t sweep_upper;
    uint8_t sweep_step;
} Instrument;

// One instrument record of the table (16 bytes each; the index is a byte shifted left as a word).

static Instrument instrument_read(const ScoModule* m, uint8_t index) {
    const uint32_t at = m->base[ScoRole_Instruments] + (uint32_t)(uint16_t)((uint32_t)index << V3_INSTRUMENT_SHIFT);
    const uint8_t vibrato = sco_u8(m, at + InstrumentField_Vibrato);
    const uint8_t steps = sco_u8(m, at + InstrumentField_VolumeSteps);
    return (Instrument) {
        .wave = sco_u8(m, at + InstrumentField_Wave),
        .sample = sco_u8(m, at + InstrumentField_Sample),
        .pitch_mode = sco_u8(m, at + InstrumentField_PitchMode),
        .vibrato_depth = (uint8_t)(vibrato >> 4),
        .vibrato_shift = (uint8_t)(vibrato & 0xF),
        .vibrato_delay = sco_u8(m, at + InstrumentField_VibratoDelay),
        .volume = sco_u8(m, at + InstrumentField_Volume),
        .volume_target = sco_u8(m, at + InstrumentField_VolumeTarget),
        .volume_step = (uint8_t)(steps >> 4),
        .volume_interval = (uint8_t)(steps & 0xF),
        .volume_delay = sco_u8(m, at + InstrumentField_VolumeDelay),
        .sweep_pos = sco_u16(m, at + InstrumentField_SweepPos),
        .sweep_lower = sco_u16(m, at + InstrumentField_SweepLower),
        .sweep_upper = sco_u16(m, at + InstrumentField_SweepUpper),
        .sweep_step = sco_u8(m, at + InstrumentField_SweepStep),
    };
}

// The period of a note (the table is indexed by a word: the note doubled).

static uint16_t note_period(const ScoModule* m, uint8_t note) {
    return sco_u16_indexed(m, m->base[ScoRole_Period], (uint16_t)(note * 2));
}

// The period of the note below `note` (the table word in front of its entry).

static uint16_t note_period_below(const ScoModule* m, uint8_t note) {
    return sco_u16_indexed(m, m->base[ScoRole_Period] - 2, (uint16_t)(note * 2));
}

// A sample of the sample table (the entries are 4 bytes; the index is scaled in a word).

static ScoSampleBounds sample_bounds(const ScoModule* m, uint8_t index) {
    return sco_sample_bounds(m, (uint16_t)(index << 2));
}

typedef struct Svx {
    bool valid;
    uint32_t oneshot;
    int32_t repeat;
    uint32_t body;
} Svx;

// An 8SVX sample: the header the replayer reads (the chunk id at +12, the one-shot and repeat lengths).

static Svx svx_read(const ScoModule* m, uint32_t start) {
    Svx svx = { 0 };
    svx.valid = sco_u32(m, start + V3_VHDR_AT) == V3_VHDR;
    svx.oneshot = sco_u32(m, start + V3_VHDR_ONESHOT);
    svx.repeat = (int32_t)sco_u32(m, start + V3_VHDR_REPEAT);
    svx.body = start + V3_VHDR_BODY;
    return svx;
}

// Volume is staged as the byte the replayer writes (high byte 0); the glue's patch masks it to 7 bits.

static void stage_silence(ScoRegisters* staged) {
    staged->lc = 0;
    staged->len_words = 0;
    staged->volume = 0;
}

// The noise buffer and its generator. The generator keeps its seed words behind the buffer, and in the build that fills
// half a buffer per call, the first word it writes ends on the seed's high byte: the seed lives in the image too.

static void noise_generate(ScoModule* m) {
    const uint32_t seed = sco_seed_address(m);
    uint32_t buffer = sco_noise_address(m);
    uint32_t last = (uint32_t)m->version->noise_words * 2 - 2;
    if (m->version->noise_alternates) {
        const uint16_t flag = (uint16_t)(sco_u16(m, seed + 4) ^ 1);
        sco_write16(m, seed + 4, flag);
        if (flag != 0) {
            buffer += 0x40;
        }
        last = 0x3F;
    }
    for (int32_t offset = (int32_t)last; offset >= 0; offset -= 2) {
        const uint16_t x = sco_u16(m, seed);
        const uint16_t y = sco_u16(m, seed + 2);
        const uint16_t value = (uint16_t)(x + x + y + x);
        sco_write16(m, seed, (uint16_t)(value + 2));
        sco_write16(m, seed + 2, (uint16_t)(y + 1));
        sco_write16(m, buffer + (uint32_t)offset, value);
    }
}

// The volume envelope of a voice, then the master level it is held under and the fade out of that level.

static void envelope_step(Sco3Voice* v) {
    Sco3Envelope* e = &v->envelope;
    bool settled = e->volume == e->target;
    if (!settled) {
        if (e->delay != 0) {
            e->delay--;
        } else if (e->counter != e->interval) {
            e->counter++;
        } else {
            e->counter = 0;
            if (e->volume < e->target) {
                e->volume = (int8_t)(e->volume + (int8_t)e->step);
                if (e->volume > e->target) {
                    e->volume = e->target;
                }
            } else {
                e->volume = (int8_t)(e->volume - (int8_t)e->step);
                if (e->volume < e->target) {
                    e->volume = e->target;
                }
            }
        }
    }
    v->staged.volume = (uint8_t)e->volume;
    if (e->limit < e->volume) {
        e->volume = e->limit;
        v->staged.volume = (uint8_t)e->limit;
    }
    if (e->fade_every == 0) {
        return;
    }
    if (e->fade_counter != e->fade_every) {
        e->fade_counter++;
        return;
    }
    e->fade_counter = 0;
    e->limit = (int8_t)(e->limit - 1);
    if (e->limit < 0) {
        e->limit = 0;
        v->staged.volume = 0;
    }
}

// A sample that loops: its repeat part (the last word when it has none).

static void sample_loop(const Sco3* s, Sco3Voice* v) {
    const ScoModule* m = s->module;
    const ScoSampleBounds bounds = sample_bounds(m, v->sample);
    const Svx svx = svx_read(m, bounds.start);
    if (svx.valid && svx.repeat > V3_SAMPLE_LOOP_MIN) {
        v->staged.lc = svx.body + svx.oneshot;
        v->staged.len_words = (uint16_t)((uint16_t)svx.repeat >> 1);
        return;
    }
    v->staged.lc = bounds.end - 2;
    v->staged.len_words = 1;
}

// A sample played in chunks: a chunk per tick from the stream position, then the loop or the end.

static void stream_chunk(Sco3Voice* v) {
    Sco3Stream* st = &v->stream;
    if (!st->active) {
        v->staged.lc = st->silence;
        v->staged.len_words = 1;
        return;
    }
    const uint32_t chunk_bytes = (uint32_t)st->chunk_words * 2;
    const int32_t room = (int32_t)(st->end - chunk_bytes);
    if ((int32_t)st->position > room) {
        if (st->loop == 0) {
            v->staged.lc = st->silence;
            v->staged.len_words = 1;
            st->active = false;
            return;
        }
        st->position = st->loop;
    }
    v->staged.lc = st->position;
    v->staged.len_words = st->chunk_words;
    st->position += st->chunk_step;
}

typedef struct WaveForm {
    uint32_t base;
    uint16_t words;
    uint32_t shift;
} WaveForm;

// The size of the wave forms a sequence or a sweep uses (the selector is the sequence's first word or the wave type):
// 8 bytes, 16 bytes, else 32 bytes. A wave is read at its number shifted by `shift` from `base`.

static WaveForm wave_form(const ScoModule* m, uint16_t selector) {
    if (selector == 8) {
        return (WaveForm) { .base = m->base[ScoRole_Wave8], .words = 4, .shift = 3 };
    }
    if (selector == 0x10) {
        return (WaveForm) { .base = m->base[ScoRole_Wave16], .words = 8, .shift = 4 };
    }
    return (WaveForm) { .base = m->base[ScoRole_Wave16], .words = 0x10, .shift = 5 };
}

// A wave form sequence: the next record of the sequence sets the wave form and the period; a record that is 0xFFFF
// jumps to the record number in its period field. Switching to or from the noise wave restarts the voice's DMA.

static void wave_sequence_step(Sco3* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco3Voice* v = &s->voice[index];
    Sco3WaveSequence* w = &v->waves;
    for (uint32_t guard = 0; guard < V3_RESTART_BUDGET; guard++) {
        w->id = instrument_read(m, v->instrument).sample;
        const uint32_t sequence = m->base[ScoRole_InstWaveBase]
                                  + sco_u32_indexed(m, m->base[ScoRole_InstWaveTable], (uint16_t)(w->id << 2));
        const WaveForm wave = wave_form(m, sco_u16(m, sequence));
        w->previous = w->current;
        const uint32_t record = sequence + ((uint32_t)w->index << 2);
        w->current = sco_u16(m, record + 2);
        const uint16_t doubled = (uint16_t)(sco_u16(m, record + 4) * 2);
        v->period = sco_u16_indexed(m, m->base[ScoRole_Period], doubled);
        if (w->current == V3_WAVE_JUMP) {
            w->index = (uint8_t)(doubled >> 1);
            continue;
        }
        if (w->current == V3_NOISE_POS) {
            if (w->previous != V3_NOISE_POS) {
                s->restart_next[index] = true;
            }
            v->staged.len_words = m->version->noise_words;
            v->staged.lc = sco_noise_address(m);
        } else {
            if (w->previous == V3_NOISE_POS) {
                s->restart_next[index] = true;
            }
            v->staged.lc = wave.base + (uint16_t)(w->current << wave.shift);
            v->staged.len_words = wave.words;
        }
        w->index++;
        return;
    }
    s->unmodeled = true;
}

// A swept wave form: the position moves by the step and turns at the limits; the wave form is read from the position.

static void sweep_step(Sco3* s, Sco3Voice* v) {
    const ScoModule* m = s->module;
    Sco3Sweep* sw = &v->sweep;
    const WaveForm wave = wave_form(m, sw->type);
    for (uint32_t guard = 0; guard < V3_RESTART_BUDGET; guard++) {
        const bool down = (sw->step & 0x80) != 0;
        const uint16_t amount = sw->step & 0x7F;
        if (down) {
            if (sw->pos == V3_NOISE_POS) {
                return;
            }
            sw->pos = (uint16_t)(sw->pos - amount);
            if ((int16_t)sw->pos >= (int16_t)sw->lower) {
                v->staged.len_words = wave.words;
                v->staged.lc = wave.base + (uint16_t)(sw->pos << wave.shift);
                return;
            }
        } else {
            sw->pos = (uint16_t)(sw->pos + amount);
            if ((int16_t)sw->pos <= (int16_t)sw->upper) {
                v->staged.len_words = wave.words;
                v->staged.lc = wave.base + (uint16_t)(sw->pos << wave.shift);
                return;
            }
        }
        sw->step ^= 0x80;
    }
    s->unmodeled = true;
}

// What the voice's wave type does every tick.

static void wave_step(Sco3* s, uint32_t index) {
    Sco3Voice* v = &s->voice[index];
    switch (v->sweep.type) {
        case Sco3Wave_Sample:
            sample_loop(s, v);
            break;
        case Sco3Wave_Sequence:
            wave_sequence_step(s, index);
            break;
        case Sco3Wave_Stream:
            stream_chunk(v);
            break;
        default:
            sweep_step(s, v);
            break;
    }
}

// A pitch slide to the note the pattern asked for: the period moves by the rate each tick up to the target's period.

static void slide_step(const ScoModule* m, Sco3Voice* v) {
    Sco3Slide* sl = &v->slide;
    if (sl->delay != 0) {
        sl->delay--;
        return;
    }
    const uint16_t target = note_period(m, sl->target);
    if ((int8_t)sl->target < (int8_t)sl->start) {
        sl->period = (uint16_t)(sl->period + sl->rate);
        if ((int16_t)sl->period <= (int16_t)target) {
            v->period = sl->period;
            return;
        }
    } else {
        sl->period = (uint16_t)(sl->period - sl->rate);
        if ((int16_t)sl->period >= (int16_t)target) {
            v->period = sl->period;
            return;
        }
    }
    sl->active = false;
    v->period = target;
}

// Vibrato: after the delay the period swings by a step (the distance to the next note shifted right) and half of it.

static void vibrato_step(const ScoModule* m, Sco3Voice* v) {
    Sco3Vibrato* vb = &v->vibrato;
    if (vb->delay != 0) {
        vb->delay--;
        return;
    }
    uint16_t step = (uint16_t)(note_period_below(m, v->note) - note_period(m, v->note));
    step = (uint16_t)(step >> (vb->shift & 7));
    if (step == 0) {
        step = 1;
    }
    const uint8_t cycle = (uint8_t)(vb->depth + vb->depth);
    if (cycle == vb->phase) {
        vb->phase = 0;
        v->period = note_period(m, v->note);
        return;
    }
    if ((int8_t)vb->phase <= (int8_t)vb->depth) {
        v->period = (uint16_t)(v->period + step);
        step = (uint16_t)(step >> 1);
        v->period = (uint16_t)(v->period - step);
    } else {
        v->period = (uint16_t)(v->period - step);
        step = (uint16_t)(step >> 1);
        v->period = (uint16_t)(v->period + step);
    }
    vb->phase++;
}

// A pitch sequence (arpeggio): bytes of notes, relative to the voice's note or absolute (bit 7), 0xFF jumps to the
// position in the next byte, 0xFE ends it.

static void pitch_sequence_step(Sco3* s, Sco3Voice* v) {
    const ScoModule* m = s->module;
    const uint32_t sequence
        = m->base[ScoRole_SeqBase] + sco_u32_indexed(m, m->base[ScoRole_SeqTable], (uint16_t)(v->pitch_sequence << 2));
    for (uint32_t guard = 0; guard < V3_RESTART_BUDGET; guard++) {
        const uint8_t entry = sco_u8(m, sequence + v->pitch_pos);
        if (entry == 0xFF) {
            v->pitch_pos = sco_u8(m, sequence + v->pitch_pos + 1);
            continue;
        }
        if (entry == 0xFE) {
            v->pitch_mode = PitchMode_None;
            return;
        }
        const uint8_t note = (entry & 0x80) != 0 ? (uint8_t)(entry & 0x7F) : (uint8_t)(entry + v->note);
        v->period = note_period(m, note);
        v->pitch_pos++;
        return;
    }
    s->unmodeled = true;
}

// The first registers of a sample instrument at a note: the whole sample from the sample offset, then (the next
// ticks) its loop.

static void sample_start(const Sco3* s, Sco3Voice* v) {
    const ScoModule* m = s->module;
    const ScoSampleBounds bounds = sample_bounds(m, v->sample);
    const Svx svx = svx_read(m, bounds.start);
    if (svx.valid) {
        const uint32_t from = svx.body + v->sample_offset;
        v->staged.lc = from;
        const uint32_t remaining = svx.oneshot + (uint32_t)svx.repeat - v->sample_offset;
        v->staged.len_words = (uint16_t)(remaining >> 1);
        return;
    }
    const uint32_t from = bounds.start + v->sample_offset;
    v->staged.lc = from;
    v->staged.len_words = (uint16_t)((bounds.end - 2 - from) >> 1);
}

// A sample instrument played in chunks: the chunk registers and the end, loop and silence addresses.

static void stream_start(const Sco3* s, Sco3Voice* v) {
    const ScoModule* m = s->module;
    Sco3Stream* st = &v->stream;
    const ScoSampleBounds bounds = sample_bounds(m, v->sample);
    const Svx svx = svx_read(m, bounds.start);
    st->active = true;
    st->end = bounds.end;
    st->silence = bounds.end - 2;
    st->loop = 0;
    if (svx.valid) {
        const uint32_t body = svx.body;
        if (svx.repeat > V3_SAMPLE_LOOP_MIN) {
            st->position = body;
            st->loop = body + svx.oneshot;
            st->end = body + svx.oneshot + (uint32_t)svx.repeat;
        } else {
            st->position = body;
        }
    } else {
        st->position = bounds.start;
    }
    v->staged.lc = st->position;
    v->staged.len_words = st->chunk_words;
}

// A note with an instrument: the instrument's fields, the volume envelope, the wave type's first registers.

static void note_instrument(Sco3* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco3Voice* v = &s->voice[index];
    const Instrument in = instrument_read(m, v->instrument);
    v->pitch_mode = in.pitch_mode;
    if (v->pitch_mode == PitchMode_Vibrato) {
        v->vibrato = (Sco3Vibrato) {
            .phase = 0, .depth = in.vibrato_depth, .shift = in.vibrato_shift, .delay = in.vibrato_delay
        };
    }
    v->pitch_pos = 0;
    Sco3Envelope* e = &v->envelope;
    e->volume = (int8_t)in.volume;
    v->staged.volume = in.volume;
    e->target = (int8_t)in.volume_target;
    e->delay = in.volume_delay;
    e->step = in.volume_step;
    e->interval = in.volume_interval;
    e->counter = 0;
    if ((int8_t)in.volume >= e->limit) {
        v->staged.volume = (uint8_t)e->limit;
    }
    if (!v->no_retrigger) {
        s->restart_next[index] = true;
    }
    v->sweep.type = in.wave;
    switch (in.wave) {
        case Sco3Wave_Sample:
            if (v->no_retrigger) {
                return;
            }
            v->sample = in.sample;
            sample_start(s, v);
            return;
        case Sco3Wave_Sequence:
            v->waves.index = 0;
            wave_sequence_step(s, index);
            return;
        case Sco3Wave_Stream:
            v->sample = in.sample;
            stream_start(s, v);
            return;
        default:
            break;
    }
    const WaveForm wave = wave_form(m, in.wave);
    v->staged.len_words = wave.words;
    v->sweep.pos = in.sweep_pos;
    if (v->sweep.pos == V3_NOISE_POS) {
        v->staged.lc = sco_noise_address(m);
        v->staged.len_words = m->version->noise_words;
        return;
    }
    v->staged.lc = wave.base + (uint32_t)(int32_t)(int16_t)(uint16_t)(in.sweep_pos << wave.shift);
    v->sweep.lower = in.sweep_lower;
    v->sweep.upper = in.sweep_upper;
    v->sweep.step = in.sweep_step;
}

// The byte `offset` behind `pos` in a pattern: the position is a word index, sign extended (`$1(a0,d4.w)`).

static uint8_t pattern_byte(const ScoModule* m, uint32_t pattern, uint16_t pos, uint16_t offset) {
    return sco_u8(m, pattern + (uint32_t)(int32_t)(int16_t)pos + offset);
}

// The bytes a pattern command takes (a note, a rest and a duration take one). A slide is followed by its note.

static uint16_t pattern_command_size(const ScoModule* m, uint8_t command) {
    switch (command) {
        case PatternCommand_Slide:
            return 4;
        case PatternCommand_Instrument:
        case PatternCommand_Adkcon:
            return 2;
        case PatternCommand_PitchSequence:
            return m->version->pitch_command_repeats ? 4 : 2;
        case PatternCommand_SampleOffset:
        case PatternCommand_Envelope:
        case PatternCommand_StreamChunk:
            return 5;
        default:
            return 1;
    }
}

// Whether a pattern byte ends an event: an envelope command, a slide (which carries its note), a rest or a note.

static bool pattern_command_ends_event(uint8_t command) {
    return command == PatternCommand_Envelope || command == PatternCommand_Slide || command < V3_NOTE_LENGTH_BASE;
}

// The bytes of a pattern command as the event sees them: a slide takes its note byte along.

static uint16_t pattern_event_size(const ScoModule* m, uint8_t command) {
    return (uint16_t)(pattern_command_size(m, command) + (command == PatternCommand_Slide ? 1 : 0));
}

// What a pattern byte did: the event goes on, the event is over, or the byte is a note to play.

typedef enum PatternStep {
    PatternStep_Next,
    PatternStep_EventDone,
    PatternStep_Note,
} PatternStep;

// A pattern command that is not a note: runs it and moves `pos` behind it.

static PatternStep pattern_command(Sco3* s, uint32_t index, uint32_t pattern, uint16_t* pos) {
    const ScoModule* m = s->module;
    Sco3Voice* v = &s->voice[index];
    const uint8_t command = pattern_byte(m, pattern, *pos, 0);
    const uint8_t arg1 = pattern_byte(m, pattern, *pos, 1);
    const uint8_t arg2 = pattern_byte(m, pattern, *pos, 2);
    const uint8_t arg3 = pattern_byte(m, pattern, *pos, 3);
    const uint8_t arg4 = pattern_byte(m, pattern, *pos, 4);
    const uint16_t size = pattern_command_size(m, command);
    switch (command) {
        case PatternCommand_Slide:
            v->slide.active = true;
            v->slide.start = (uint8_t)(arg1 + v->transpose);
            v->slide.rate = arg2;
            v->slide.delay = arg3;
            v->slide.period = note_period(m, v->slide.start);
            *pos = (uint16_t)(*pos + size);
            return PatternStep_Note;
        case PatternCommand_Instrument:
            v->instrument = (uint8_t)(arg1 + v->instrument_offset);
            break;
        case PatternCommand_PitchSequence:
            v->pitch_sequence = arg1;
            if (m->version->pitch_command_repeats) {
                // the build's branch after the command lands in the instrument command, past the load of the
                // instrument field: it adds the instrument offset to the byte after the next one and stores the sum
                // in the pitch sequence field again
                v->pitch_sequence = (uint8_t)(arg3 + v->instrument_offset);
            }
            break;
        case PatternCommand_SampleOffset:
            v->sample_offset = ((uint32_t)arg1 << 24) | ((uint32_t)arg2 << 16) | ((uint32_t)arg3 << 8) | arg4;
            break;
        case PatternCommand_Envelope: {
            Sco3Envelope* e = &v->envelope;
            e->volume = (int8_t)arg1;
            v->staged.volume = arg1;
            e->target = (int8_t)arg2;
            e->step = (uint8_t)(arg3 >> 4);
            e->interval = (uint8_t)(arg3 & 0xF);
            e->delay = arg4;
            e->counter = 0;
            v->length_counter = v->length;
            v->pattern_pos = (uint16_t)(*pos + size);
            return PatternStep_EventDone;
        }
        case PatternCommand_Adkcon:
            // the bits of ADKCON are the voices' modulation switches; the open-time walk refused every song that
            // sets one
            s->unmodeled = s->unmodeled || arg1 != 0;
            break;
        case PatternCommand_StreamChunk:
            v->stream.chunk_words = (uint16_t)((((uint16_t)arg1 << 8) | arg2) >> 1);
            v->stream.chunk_step = (uint16_t)(((uint16_t)arg3 << 8) | arg4);
            break;
        case PatternCommand_Marker:
            break;
        case PatternCommand_NoRetriggerOff:
        case PatternCommand_NoRetriggerOn:
            v->no_retrigger = (command & 1) == 0;
            break;
        default:
            if (command >= V3_NOTE_LENGTH_BASE) {
                v->length = (uint8_t)(command - V3_NOTE_LENGTH_BIAS);
                break;
            }
            if (command == V3_NOTE_REST) {
                v->length_counter = v->length;
                v->pattern_pos = (uint16_t)(*pos + size);
                return PatternStep_EventDone;
            }
            return PatternStep_Note;
    }
    *pos = (uint16_t)(*pos + size);
    return PatternStep_Next;
}

static void stream_next_entry(Sco3* s, uint32_t index);

// Reads the pattern from the voice's position until a note, a slide, a rest or an envelope command ends the event; at
// the end of the pattern the voice moves on in its stream.

static void pattern_run(Sco3* s, uint32_t index, uint32_t pattern) {
    const ScoModule* m = s->module;
    Sco3Voice* v = &s->voice[index];
    uint16_t pos = v->pattern_pos;
    bool event_done = false;
    for (uint32_t guard = 0; guard < V3_RESTART_BUDGET && !event_done; guard++) {
        const PatternStep step = pattern_command(s, index, pattern, &pos);
        if (step == PatternStep_Next) {
            continue;
        }
        event_done = true;
        if (step == PatternStep_EventDone) {
            continue;
        }
        // a note: the byte after a slide command, or this one
        const uint8_t byte = pattern_byte(m, pattern, pos, 0);
        pos = (uint16_t)(pos + 1);
        v->pattern_pos = pos;
        uint8_t note = (uint8_t)(byte + v->transpose);
        v->note = note;
        if (v->slide.active) {
            v->slide.target = note;
            note = v->slide.start;
        }
        v->length_counter = v->length;
        v->period = note_period(m, note);
        v->played = true;
        note_instrument(s, index);
    }
    if (!event_done) {
        s->unmodeled = true;
        return;
    }
    // the event is over; the end of the pattern moves the stream on
    if (pattern_byte(m, pattern, v->pattern_pos, 0) != V3_PATTERN_END) {
        return;
    }
    v->pattern_pos = 0;
    if (v->pattern_repeat != 0) {
        v->pattern_repeat--;
        return;
    }
    stream_next_entry(s, index);
}

// The pattern ended: a stream word 0xFF00 behind the entry jumps (to the word index in the one after it), else the
// stream goes to the next word.

static void stream_next_entry(Sco3* s, uint32_t index) {
    Sco3Voice* v = &s->voice[index];
    if (v->stream_next.command == StreamCommand_Jump && v->stream_next.arg == 0) {
        v->stream_pos = (uint16_t)(v->stream_jump + v->stream_jump);
        // the eagleplayer patched this jump of the module to call SongEnd when voice 0 takes it
        if (index == 0) {
            s->song_end = true;
        }
    } else {
        v->stream_pos = (uint16_t)(v->stream_pos + 2);
    }
}

// The stream entry `offset` bytes behind `pos`: the position is a word index, sign extended (`2(a1,d4.w)`).

static ScoStreamEntry stream_entry(const ScoModule* m, uint32_t base, uint16_t pos, uint16_t offset) {
    const uint16_t word = sco_u16(m, base + (uint32_t)(int32_t)(int16_t)pos + offset);
    return (ScoStreamEntry) { .word = word, .command = (uint8_t)(word >> 8), .arg = (uint8_t)word };
}

// Whether a stream entry is the control command `command` (those take a zero argument).

static bool stream_control(ScoStreamEntry entry, StreamCommand command) {
    return entry.command == command && entry.arg == 0;
}

// The note counters ran out: read the stream. Control words change the voice and read on; a pattern number starts the
// pattern at the voice's position.

static void sequence_advance(Sco3* s, uint32_t index) {
    const ScoModule* m = s->module;
    Sco3Voice* v = &s->voice[index];
    uint16_t pos = v->stream_pos;
    for (uint32_t guard = 0; guard < V3_RESTART_BUDGET; guard++) {
        ScoStreamEntry entry = stream_entry(m, v->stream_base, pos, 0);
        if (stream_control(entry, StreamCommand_Jump)) {
            pos = stream_entry(m, v->stream_base, pos, 2).word;
            pos = (uint16_t)(pos + pos);
            entry = stream_entry(m, v->stream_base, pos, 0);
        }
        if (stream_control(entry, StreamCommand_End)) {
            v->active = false;
            stage_silence(&v->staged);
            return;
        }
        if (entry.command == StreamCommand_Speed) {
            v->speed = entry.arg;
            v->speed_counter = entry.arg;
            pos = (uint16_t)(pos + 2);
            continue;
        }
        if (entry.command == StreamCommand_Transpose) {
            v->transpose = entry.arg;
            pos = (uint16_t)(pos + 2);
            entry = stream_entry(m, v->stream_base, pos, 0);
        }
        if (entry.command == StreamCommand_InstrumentOffset) {
            v->instrument_offset = entry.arg;
            pos = (uint16_t)(pos + 2);
            continue;
        }
        if (entry.command == StreamCommand_MasterVolume) {
            for (uint32_t i = 0; i < SCO_VOICES; i++) {
                s->voice[i].envelope.limit = V3_MAX_VOLUME;
                s->voice[i].envelope.fade_counter = 0;
                s->voice[i].envelope.fade_every = entry.arg;
            }
            pos = (uint16_t)(pos + 2);
            continue;
        }
        if (stream_control(entry, StreamCommand_Marker)) {
            pos = (uint16_t)(pos + 2);
            continue;
        }
        if (entry.command == StreamCommand_PatternRepeat) {
            v->pattern_repeat = (uint8_t)(entry.arg - 1);
            pos = (uint16_t)(pos + 2);
            continue;
        }
        if (entry.command == StreamCommand_LoopStart) {
            v->loop_count = (int8_t)(entry.arg - 1);
            pos = (uint16_t)(pos + 2);
            v->loop_pos = pos;
            continue;
        }
        if (stream_control(entry, StreamCommand_LoopEnd)) {
            v->loop_count = (int8_t)(v->loop_count - 1);
            pos = v->loop_count >= 0 ? v->loop_pos : (uint16_t)(pos + 2);
            continue;
        }
        v->stream_pos = pos;
        v->stream_next = stream_entry(m, v->stream_base, pos, 2);
        v->stream_jump = stream_entry(m, v->stream_base, pos, 4).word;
        v->slide.active = false;
        const uint16_t number = (uint16_t)(entry.word << 2);
        const uint32_t pattern
            = sco_u32_indexed(m, m->base[ScoRole_PatternTable], number) + m->base[ScoRole_PatternData];
        pattern_run(s, index, pattern);
        return;
    }
    s->unmodeled = true;
}

// One voice of a tick.

static void voice_run(Sco3* s, uint32_t index, NuadePaulaChannel* paula) {
    Sco3Voice* v = &s->voice[index];
    if (!v->active) {
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
    wave_step(s, index);
    envelope_step(v);
    if (v->slide.active) {
        slide_step(s->module, v);
        return;
    }
    if (v->pitch_mode == PitchMode_None) {
        return;
    }
    if (v->pitch_mode == PitchMode_Vibrato) {
        vibrato_step(s->module, v);
    } else {
        pitch_sequence_step(s, v);
    }
}

void sc3_tick(Sco3* s, NuadePaulaChannel* paula) {
    s->song_end = false;
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->restart[i]) {
            nuade_paula_channel_dma_off(&paula[i]);
        }
    }
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        sco_registers_write(s->module, &s->voice[i].written, &paula[i]);
    }
    for (int32_t i = SCO_VOICES - 1; i >= 0; i--) {
        voice_run(s, (uint32_t)i, paula);
        s->voice[i].staged.period = s->voice[i].period;
    }
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        s->voice[i].written = s->voice[i].staged;
    }
    const uint32_t noise = sco_noise_address(s->module);
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->voice[i].written.lc == noise) {
            noise_generate(s->module);
            break;
        }
    }
    if (sc3_ended(s)) {
        return;
    }
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->restart[i]) {
            nuade_paula_channel_dma_on(&paula[i]);
        }
        s->restart[i] = s->restart_next[i];
        s->restart_next[i] = false;
    }
}

bool sc3_ended(const Sco3* s) {
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        if (s->voice[i].active) {
            return false;
        }
    }
    return true;
}

// The init routine: the replayer's variables are cleared, the voices get their streams and speed, the pattern base and
// the noise seed are set, DMA is off and the period and volume registers are 0.

void sc3_init(Sco3* s, ScoModule* module, uint32_t subsong, NuadePaulaChannel* paula) {
    memset(s, 0, sizeof(*s));
    s->module = module;
    const uint32_t song = module->base[ScoRole_Songs] + (subsong & (SCO_MAX_SUBSONGS - 1)) * SCO_SONG_BYTES;
    for (uint32_t i = 0; i < SCO_VOICES; i++) {
        Sco3Voice* v = &s->voice[i];
        v->active = true;
        v->speed = V3_SPEED_INITIAL;
        v->speed_counter = 1;
        v->length = 1;
        v->length_counter = 1;
        v->pitch_mode = PitchMode_Vibrato;
        v->envelope.limit = V3_MAX_VOLUME;
        v->stream_base = module->base[ScoRole_Streams] + sco_u32(module, song + i * 4);
        nuade_paula_channel_dma_off(&paula[i]);
        paula[i].period = 0;
        paula[i].volume = 0;
    }
    const uint32_t noise = sco_noise_address(module);
    const uint32_t seed = sco_seed_address(module);
    for (uint32_t at = noise; at < seed + 6; at++) {
        sco_write8(module, at, 0);
    }
    sco_write8(module, seed, SCO_V3_TOD);
}

// The open-time proof that the module only does what the port models. The song tables, the streams of every subsong and
// the patterns they name are walked as the replayer reads them, under one work budget for the whole module (a module
// that runs it out is refused). What it checks:
// - every stream and pattern lies behind the code (not in the replayer's variables);
// - no pattern writes a nonzero value to ADKCON, which switches Paula's voice-to-voice modulation on.
// A stream's state is its position and what the previous word allows (after a jump the word is not tested for a jump
// again, after a transposition not for a jump, end, speed or transposition); a loop end may go back to any loop start,
// so it is walked as going on: the loop start's own successor is the loop's first word.

#define V3_WALK_BUDGET 0x400000u
#define V3_STREAM_STATES (0x10000u * 4u)
#define V3_PATTERN_NUMBERS 0x10000u
#define V3_FLAG_AFTER_JUMP 1u
#define V3_FLAG_AFTER_TRANSPOSE 2u

typedef struct Walk {
    const ScoModule* m;
    uint32_t budget;
    uint8_t* visited; // one bit per stream state of the voice being walked
    uint32_t* stack;  // the stream states to walk
    uint32_t depth;
    uint8_t* patterns; // one bit per pattern number a stream names
} Walk;

static void walk_push(Walk* w, uint16_t pos, uint32_t flags) {
    const uint32_t state = ((uint32_t)pos << 2) | flags;
    if ((w->visited[state >> 3] & (1u << (state & 7))) == 0) {
        w->visited[state >> 3] |= (uint8_t)(1u << (state & 7));
        w->stack[w->depth++] = state;
    }
}

// The states reachable from one stream state, as sequence_advance moves; false when the work budget is gone.

static bool walk_stream(Walk* w, uint32_t base) {
    memset(w->visited, 0, V3_STREAM_STATES / 8);
    w->depth = 0;
    walk_push(w, 0, 0);
    while (w->depth > 0) {
        if (w->budget == 0) {
            return false;
        }
        w->budget--;
        const uint32_t state = w->stack[--w->depth];
        const uint16_t pos = (uint16_t)(state >> 2);
        const uint32_t flags = state & 3;
        const ScoStreamEntry entry = stream_entry(w->m, base, pos, 0);
        const uint16_t next = (uint16_t)(pos + 2);
        if (flags != V3_FLAG_AFTER_TRANSPOSE) {
            if (flags != V3_FLAG_AFTER_JUMP && stream_control(entry, StreamCommand_Jump)) {
                const uint16_t target = stream_entry(w->m, base, pos, 2).word;
                walk_push(w, (uint16_t)(target + target), V3_FLAG_AFTER_JUMP);
                continue;
            }
            if (stream_control(entry, StreamCommand_End)) {
                continue;
            }
            if (entry.command == StreamCommand_Speed) {
                walk_push(w, next, 0);
                continue;
            }
            if (entry.command == StreamCommand_Transpose) {
                walk_push(w, next, V3_FLAG_AFTER_TRANSPOSE);
                continue;
            }
        }
        const bool marker = stream_control(entry, StreamCommand_Marker) || stream_control(entry, StreamCommand_LoopEnd);
        if (entry.command == StreamCommand_InstrumentOffset || entry.command == StreamCommand_MasterVolume
            || entry.command == StreamCommand_PatternRepeat || entry.command == StreamCommand_LoopStart || marker) {
            walk_push(w, next, 0);
            continue;
        }
        // a pattern number: the stream goes on behind the entry, or jumps when the next word is a jump
        w->patterns[entry.word >> 3] |= (uint8_t)(1u << (entry.word & 7));
        walk_push(w, next, 0);
        if (stream_control(stream_entry(w->m, base, pos, 2), StreamCommand_Jump)) {
            const uint16_t target = stream_entry(w->m, base, pos, 4).word;
            walk_push(w, (uint16_t)(target + target), 0);
        }
    }
    return true;
}

// A pattern as the replayer reads it (an event is a note, a slide, a rest or an envelope command; after it, 0xFF ends
// the pattern).

static bool walk_pattern(Walk* w, uint32_t pattern) {
    uint16_t pos = 0;
    for (;;) {
        if (w->budget == 0) {
            return false;
        }
        w->budget--;
        const uint8_t command = pattern_byte(w->m, pattern, pos, 0);
        if (command == PatternCommand_Adkcon && pattern_byte(w->m, pattern, pos, 1) != 0) {
            return false;
        }
        pos = (uint16_t)(pos + pattern_event_size(w->m, command));
        if (pattern_command_ends_event(command) && pattern_byte(w->m, pattern, pos, 0) == V3_PATTERN_END) {
            return true;
        }
    }
}

static bool behind_code(const ScoModule* m, uint32_t address) {
    return address >= m->entry + m->version->code_len && address < m->size;
}

static bool walk_module(Walk* w) {
    const ScoModule* m = w->m;
    for (uint32_t subsong = 0; subsong < m->subsong_count; subsong++) {
        for (uint32_t voice = 0; voice < SCO_VOICES; voice++) {
            const uint32_t entry = m->base[ScoRole_Songs] + subsong * SCO_SONG_BYTES + voice * 4;
            const uint32_t base = m->base[ScoRole_Streams] + sco_u32(m, entry);
            if (!behind_code(m, base) || !walk_stream(w, base)) {
                return false;
            }
        }
    }
    for (uint32_t number = 0; number < V3_PATTERN_NUMBERS; number++) {
        if ((w->patterns[number >> 3] & (1u << (number & 7))) == 0) {
            continue;
        }
        const uint32_t pattern
            = m->base[ScoRole_PatternData] + sco_u32_indexed(m, m->base[ScoRole_PatternTable], (uint16_t)(number << 2));
        if (!behind_code(m, pattern) || !walk_pattern(w, pattern)) {
            return false;
        }
    }
    return !sco_module_var_read(m);
}

bool sc3_modeled(const ScoModule* m) {
    Walk w = { .m = m, .budget = V3_WALK_BUDGET };
    w.visited = calloc(V3_STREAM_STATES / 8, 1);
    w.stack = malloc(sizeof(uint32_t) * V3_STREAM_STATES);
    w.patterns = calloc(V3_PATTERN_NUMBERS / 8, 1);
    const bool ok = w.visited != nullptr && w.stack != nullptr && w.patterns != nullptr && walk_module(&w);
    free(w.visited);
    free(w.stack);
    free(w.patterns);
    return ok;
}
