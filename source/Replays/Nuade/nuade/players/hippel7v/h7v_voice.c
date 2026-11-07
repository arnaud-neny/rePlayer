// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player: the voices. A track reads its pattern rows; a row with a note starts an instrument (a
// volume sequence), which names a frequency sequence. The frequency sequence picks samples and the note offset, the
// volume sequence the volume; on top come vibrato, portamento, a sample window sweep and a random volume reduction.
// The same code runs the three tracks that have a Paula channel and the four that are mixed in software: the mixed
// voices have "registers" of their own, which the mixer reads.

#include "h7v_internal.h"

#include <string.h>

#define SEQ_COMMAND 0xE0
#define VSEQ_HEADER 5
#define NO_FSEQ 0x80
#define SEQUENCE_GUARD 256

#define INFO_PORTAMENTO 0x20
#define INFO_ALT_FSEQ 0x40
#define INFO_INSTRUMENT 0x1F
#define PATTERN_END_NOTE 1 // a row with this note ends the pattern
#define NOTE_MASK 0x7F
#define NOTE_NO_INSTRUMENT 0x80

#define SAMPLE_PACK_RECORD 0x18
#define PLACEHOLDER_PERIOD 4 // what a started sample's period register holds until the tick's own write
#define PAULA_VOLUME_MASK 0x7F

typedef enum FseqCommand {
    FseqCommand_Jump = 0,      // E0 pos
    FseqCommand_Hold,          // E1
    FseqCommand_Sample,        // E2 sample: start it
    FseqCommand_Vibrato,       // E3 speed depth
    FseqCommand_SampleNoReset, // E4 sample: switch the playing sample at its next loop
    FseqCommand_Sweep,         // E5 sample pos.w length.w delta.w speed: start a sweeping window
    FseqCommand_SweepChange,   // E6 length.w delta.w speed
    FseqCommand_SampleIfNew,   // E7 sample: start it unless it is the current one
    FseqCommand_Wait,          // E8 ticks
    FseqCommand_PackedSample,  // E9 pack entry: start one sample of a sample pack
    FseqCommand_RandomVolume,  // EA range: a random volume reduction for the note
} FseqCommand;

typedef enum VseqCommand {
    VseqCommand_Jump = 0, // E0 pos (counted from the sequence header)
    VseqCommand_Wait = 8, // E8 ticks; E1..E7 hold
} VseqCommand;

// The sequence every voice starts with (volume and frequency): value 1, then hold.
static const uint8_t k_start_sequence[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE1 };

// AUDxLC before a voice plays anything: one silent word.
static const _Alignas(2) int8_t k_silence[2] = { 0, 0 };

static H7vSequence module_sequence(const H7vModule* m, uint32_t offset) {
    return offset < m->size ? (H7vSequence) { m->data + offset, m->size - offset, offset }
                            : (H7vSequence) { nullptr, 0, 0 };
}

static uint8_t seq_byte(H7vSequence s, int32_t pos) {
    if (pos < 0) {
        return (uint32_t)-pos <= s.before ? s.data[pos] : 0;
    }
    return (uint32_t)pos < s.size ? s.data[pos] : 0;
}

static uint16_t seq_word(H7vSequence s, int32_t pos) {
    return (uint16_t)(seq_byte(s, pos) << 8 | seq_byte(s, pos + 1));
}

void h7v_voices_init(H7vSong* song, const H7vModule* module, const H7vSubsong* subsong) {
    song->speed = subsong->speed;
    song->speed_counter = 1;
    for (int i = 0; i < H7V_TRACKS; i++) {
        H7vVoice* v = &song->voices[i];
        *v = (H7vVoice) { 0 };
        v->mixed_slot = i < H7V_PAULA_TRACKS ? -1 : i - H7V_PAULA_TRACKS;
        v->fseq = (H7vSequence) { k_start_sequence, sizeof(k_start_sequence), 0 };
        v->vseq = v->fseq;
        v->vseq_counter = 1;
        v->vseq_speed = 1;
        v->volume_percent = HIP_VOLUME_FULL;
        v->sample_number = 0xFF;
        v->pattern = module->patterns;
        v->pattern_pos = module->pattern_size;
        v->last_note = -1;
        v->last_instrument = -1;
        v->track_start
            = module->tracks + (uint32_t)subsong->first * H7V_TRACK_ROW_SIZE + (uint32_t)i * H7V_TRACK_ENTRY_SIZE;
        v->track_end = module->tracks + ((uint16_t)(subsong->last + 1)) * H7V_TRACK_ROW_SIZE
                       + (uint32_t)i * H7V_TRACK_ENTRY_SIZE;
        v->loop = k_silence;
        v->loop_length = 1;
        v->paula.lc = k_silence;
        v->paula.len_words = 1;
    }
}

// The next word of the random generator

static uint16_t random_next(H7vSong* s) {
    const uint16_t sum = (uint16_t)(s->random_state + 0x4793);
    const uint16_t rotated = (uint16_t)((sum >> 6) | (sum << 10));
    s->random_state = (uint16_t)(rotated ^ sum);
    return s->random_state;
}

// Sequencer: the track list and the pattern rows

static void read_track_entry(const H7vModule* m, H7vSong* s, H7vVoice* v, H7vSequencerOutput* out) {
    uint32_t at = v->track_start + (uint32_t)(int32_t)(int16_t)v->track_pos;
    if (at == v->track_end) {
        v->track_pos = 0;
        at = v->track_start;
        s->looped = true;
    }
    const H7vTrackEntry entry = h7v_track_entry(m, at);
    v->note_transpose = entry.note_transpose;
    v->instrument_transpose = entry.instrument_transpose;
    switch ((H7vTrackEffect)entry.effect) {
        case H7vTrackEffect_Tempo:
            out->tempo_changed = true;
            out->tempo_offset = h7v_track_entry(m, at + H7V_TRACK_ENTRY_SIZE).tempo;
            break;
        case H7vTrackEffect_Volume:
            v->volume_percent = hip_level_percent(entry.param);
            break;
        case H7vTrackEffect_Stop:
            s->stopped = true;
            break;
        case H7vTrackEffect_Speed:
            s->speed = entry.param;
            break;
        default:
            break;
    }
    v->pattern_pos = 0;
    v->pattern = m->patterns + (uint32_t)entry.pattern * m->pattern_size;
    v->track_pos = (uint16_t)(v->track_pos + H7V_TRACK_ROW_SIZE);
}

// The row's note starts an instrument: its volume sequence, vibrato and frequency sequence.

static void start_instrument(const H7vModule* m, H7vVoice* v) {
    const uint8_t instrument = (uint8_t)((v->info & INFO_INSTRUMENT) + v->instrument_transpose);
    const uint32_t record = m->vseqs + ((uint32_t)instrument << 6);
    v->last_note = (v->note + v->note_transpose) & NOTE_MASK;
    v->last_instrument = instrument;
    v->note_starts++;
    v->vseq_pos = 0;
    v->vseq_counter = h7v_byte(m, record);
    v->vseq_speed = v->vseq_counter;
    uint8_t fseq = h7v_byte(m, record + 1);
    hip_vibrato_start(&v->vibrato, h7v_byte(m, record + 2), h7v_byte(m, record + 3), h7v_byte(m, record + 4));
    v->vseq = module_sequence(m, record + VSEQ_HEADER);
    v->vseq_wait = 0;
    if (fseq == NO_FSEQ) {
        return;
    }
    if (v->info & INFO_ALT_FSEQ) {
        fseq = v->info_prev;
    }
    v->fseq = module_sequence(m, m->fseqs + ((uint32_t)fseq << 6));
    v->fseq_pos = 0;
    v->fseq_wait = 0;
}

H7vSequencerOutput h7v_sequencer_step(const H7vModule* m, H7vSong* s, H7vVoice* v) {
    H7vSequencerOutput out = { 0 };
    uint32_t row = v->pattern + (uint32_t)(int32_t)(int16_t)v->pattern_pos;
    if ((h7v_byte(m, row) & NOTE_MASK) == PATTERN_END_NOTE || v->pattern_pos == m->pattern_size) {
        read_track_entry(m, s, v, &out);
        row = v->pattern;
    }
    const uint8_t note_byte = h7v_byte(m, row);
    if ((note_byte & NOTE_MASK) != 0) {
        v->porta = 0;
        v->note = note_byte & NOTE_MASK;
        // The byte before the row's note is the row before's info byte; the first row looks at the pattern's last
        const uint32_t before
            = v->pattern_pos == 0 ? row + 1 + (uint32_t)(int32_t)(int16_t)m->pattern_size - 2 : row - 1;
        v->info_prev = h7v_byte(m, before);
        v->info = h7v_byte(m, row + 1);
        if (!(note_byte & NOTE_NO_INSTRUMENT)) {
            start_instrument(m, v);
        }
    }
    v->pattern_pos = (uint16_t)(v->pattern_pos + 2);
    return out;
}

// Write AUDxLC / AUDxLEN. Paula ignores bit 0 of an address; the mixer's registers are plain memory.

static void set_sample_registers(H7vVoice* v, const int8_t* lc, uint16_t len_words) {
    v->paula.lc = v->mixed_slot < 0 ? (const int8_t*)((uintptr_t)lc & ~(uintptr_t)1) : lc;
    v->paula.len_words = len_words;
}

// The repeat of a track: AUDxLC / AUDxLEN take the loop. A Paula channel gets it after the DMA of the channels that
// started a sample went on, which latched the start of the sample.

void h7v_write_repeat(H7vVoice* v) {
    set_sample_registers(v, v->loop, v->loop_length);
}

// The channel or mixer voice stops (the sample is about to change). A Paula channel is switched on again at the end of
// the tick; a mixed voice at the mixer's next update.

static void voice_off(H7vVoice* v) {
    if (v->mixed_slot >= 0) {
        v->mixer_started = true;
        v->mixer_on = false;
        return;
    }
    nuade_paula_channel_dma_off(&v->paula);
    v->restart_dma = true;
}

// Point the registers at a sample and set up its repeat.

static void load_sample(const H7vModule* m, H7vVoice* v, uint8_t number) {
    const H7vSampleRecord e = h7v_sample_record(m, number);
    const uint32_t start = h7v_sample_offset(m, e.start);
    const H7vSampleRange sample = h7v_sample_range(m, start, e.length);
    set_sample_registers(v, sample.lc, sample.len_words);
    const H7vSampleRange loop = h7v_sample_range(m, (uint64_t)start + e.loop_offset, e.loop_length);
    v->loop = loop.lc;
    v->loop_length = loop.len_words;
}

static void restart_volume(H7vVoice* v) {
    v->vseq_pos = 0;
    v->vseq_counter = 1;
}

// One sample of a sample pack: a block with a record count, an extra size (longs), records of start/end offsets, then
// the data. The first byte of the sample is copied over the second.

static void load_packed_sample(H7vModule* m, H7vVoice* v, uint8_t number, uint8_t record) {
    const uint32_t block = h7v_sample_offset(m, h7v_sample_record(m, number).start);
    const uint32_t count = h7v_word(m, block + 4);
    // The extra size is shifted as a word and added sign-extended (lsl.w, adda.w)
    const uint32_t extra = (uint32_t)(int32_t)(int16_t)(uint16_t)(h7v_word(m, block + 6) << 2);
    const uint32_t records = block + 8;
    const uint32_t data = records + count * SAMPLE_PACK_RECORD + extra;
    const uint32_t entry = records + (uint32_t)record * SAMPLE_PACK_RECORD;
    const uint32_t start = h7v_long(m, entry) & ~1u;
    const uint32_t end = h7v_long(m, entry + 4) & ~1u;
    const uint32_t at = data + start;
    h7v_patch_second_byte(m, at);
    const H7vSampleRange sample = h7v_sample_range(m, at, (uint16_t)((end - start) >> 1));
    set_sample_registers(v, sample.lc, sample.len_words);
    v->loop = h7v_sample_range(m, at, 1).lc;
    v->loop_length = 1;
}

// Start a sample from the beginning: the channel stops, takes the sample and its repeat, the volume sequence starts
// over.

static void start_sample(const H7vModule* m, H7vVoice* v, uint8_t number) {
    voice_off(v);
    load_sample(m, v, number);
    v->paula.period = PLACEHOLDER_PERIOD;
}

// Read the frequency sequence until it yields a value or holds.

static void run_fseq(H7vModule* m, H7vVoice* v) {
    if (v->fseq_wait != 0) {
        v->fseq_wait--;
        return;
    }
    for (int guard = 0; guard < SEQUENCE_GUARD; guard++) {
        const uint32_t at = v->fseq_pos;
        const uint8_t c = seq_byte(v->fseq, at);
        if (c < SEQ_COMMAND) {
            v->fseq_value = c;
            v->fseq_pos++;
            return;
        }
        const uint8_t arg = seq_byte(v->fseq, at + 1);
        switch ((FseqCommand)(c - SEQ_COMMAND)) {
            case FseqCommand_Jump:
                v->fseq_pos = arg;
                break;
            case FseqCommand_Hold:
                return;
            case FseqCommand_Sample:
                v->random_range = 0;
                v->sample_number = 0xFF;
                start_sample(m, v, arg);
                restart_volume(v);
                v->fseq_pos += 2;
                v->sweep_active = false;
                break;
            case FseqCommand_Vibrato:
                v->vibrato.speed = arg;
                v->vibrato.depth = seq_byte(v->fseq, at + 2);
                v->fseq_pos += 3;
                break;
            case FseqCommand_SampleNoReset:
                load_sample(m, v, arg);
                v->fseq_pos += 2;
                v->sweep_active = false;
                break;
            case FseqCommand_Sweep: {
                v->random_range = 0;
                voice_off(v);
                v->paula.period = PLACEHOLDER_PERIOD;
                const H7vSampleRecord e = h7v_sample_record(m, arg);
                v->sweep_start = e.start;
                v->sweep_end = e.start + (uint32_t)e.length * 2;
                const uint16_t pos = seq_word(v->fseq, at + 2);
                v->sweep_pos = pos == 0xFFFF ? e.length : pos;
                v->sweep_length = seq_word(v->fseq, at + 4);
                v->sweep_delta = (int16_t)seq_word(v->fseq, at + 6);
                v->sweep_counter = 0;
                v->sweep_speed = seq_byte(v->fseq, at + 8);
                v->sweep_stopped = false;
                v->sweep_started = false;
                v->sweep_active = true;
                restart_volume(v);
                v->fseq_pos += 9;
                break;
            }
            case FseqCommand_SweepChange:
                v->sweep_length = seq_word(v->fseq, at + 1);
                v->sweep_delta = (int16_t)seq_word(v->fseq, at + 3);
                v->sweep_counter = 0;
                v->sweep_speed = seq_byte(v->fseq, at + 5);
                v->sweep_started = false;
                v->sweep_stopped = false;
                v->fseq_pos += 6;
                break;
            case FseqCommand_SampleIfNew:
                if (arg != v->sample_number) {
                    v->random_range = 0;
                    v->sample_number = arg;
                    start_sample(m, v, arg);
                }
                restart_volume(v);
                v->fseq_pos += 2;
                v->sweep_active = false;
                break;
            case FseqCommand_Wait:
                v->fseq_wait = arg;
                v->fseq_pos += 2;
                // back to the top, where the wait is counted down
                if (v->fseq_wait != 0) {
                    v->fseq_wait--;
                    return;
                }
                break;
            case FseqCommand_PackedSample:
                v->random_range = 0;
                v->sample_number = 0xFF;
                voice_off(v);
                load_packed_sample(m, v, arg, seq_byte(v->fseq, at + 2));
                v->paula.period = PLACEHOLDER_PERIOD;
                restart_volume(v);
                v->fseq_pos += 3;
                v->sweep_active = false;
                break;
            case FseqCommand_RandomVolume:
                v->random_range = arg;
                v->random_value = 0;
                v->fseq_pos += 2;
                break;
            default:
                return; // the replayer would jump into the code behind its table
        }
    }
}

// Move the sample window: AUDxLC/AUDxLEN become the repeat, no restart.

static void run_sweep(const H7vModule* m, H7vVoice* v) {
    if (!v->sweep_active || v->sweep_stopped) {
        return;
    }
    v->sweep_counter--;
    if (v->sweep_counter >= 0) {
        return;
    }
    v->sweep_counter = (int8_t)v->sweep_speed;
    int32_t pos = v->sweep_pos;
    if (!v->sweep_started) {
        v->sweep_started = true;
    } else {
        pos += v->sweep_delta;
        if (pos < 0) {
            v->sweep_stopped = true;
            pos -= v->sweep_delta;
        } else if ((uint64_t)v->sweep_start + (uint64_t)pos * 2 + (uint64_t)v->sweep_length * 2 > v->sweep_end) {
            v->sweep_stopped = true;
            pos -= v->sweep_delta;
        }
    }
    v->sweep_pos = (uint16_t)pos;
    const uint32_t window_start = h7v_sample_offset(m, (uint64_t)v->sweep_start + (uint64_t)pos * 2);
    const H7vSampleRange window = h7v_sample_range(m, window_start, v->sweep_length);
    v->loop_length = window.len_words;
    v->loop = window.lc;
    set_sample_registers(v, window.lc, window.len_words);
}

static void run_vseq(H7vVoice* v) {
    if (v->vseq_wait != 0) {
        v->vseq_wait--;
        return;
    }
    v->vseq_counter--;
    if (v->vseq_counter != 0) {
        return;
    }
    v->vseq_counter = v->vseq_speed;
    for (int guard = 0; guard < SEQUENCE_GUARD; guard++) {
        // The position is a signed word: a jump to a value below 5 goes back into the instrument's header
        const int32_t at = (int16_t)v->vseq_pos;
        const uint8_t c = seq_byte(v->vseq, at);
        if (c < SEQ_COMMAND) {
            v->volume = c;
            v->vseq_pos++;
            return;
        }
        const uint8_t arg = seq_byte(v->vseq, at + 1);
        if (c - SEQ_COMMAND == VseqCommand_Jump) {
            v->vseq_pos = (uint16_t)(arg - VSEQ_HEADER);
        } else if (c - SEQ_COMMAND == VseqCommand_Wait) {
            v->vseq_wait = arg;
            v->vseq_pos += 2;
            if (v->vseq_wait != 0) {
                v->vseq_wait--;
                return;
            }
            // a zero wait reads on at the volume counter: it was just reloaded, so it is not 0 unless the speed is 1
            v->vseq_counter--;
            if (v->vseq_counter != 0) {
                return;
            }
            v->vseq_counter = v->vseq_speed;
        } else {
            return; // E1..E7: hold
        }
    }
}

static uint16_t note_period(const H7vVoice* v) {
    uint8_t note = v->fseq_value;
    if (!(note & 0x80)) {
        note = (uint8_t)(note + v->note + v->note_transpose);
    }
    return hip_note_period(note & NOTE_MASK);
}

// The volume sequence's value in the voice's percentage, less the random reduction of the note (drawn when the first
// tick after the command asks for it).

static uint8_t voice_volume(H7vSong* s, H7vVoice* v) {
    int16_t percent = v->volume_percent;
    if (v->random_range != 0) {
        if (v->random_value == 0) {
            const uint8_t range = v->random_range;
            v->random_range = 0;
            v->random_value = (uint8_t)((uint32_t)range * (random_next(s) & 0xFF) / 0xFF);
        }
        percent = (int16_t)(percent - v->random_value);
    }
    return hip_scaled_volume(v->volume, percent < 0 ? 0 : (uint8_t)percent);
}

void h7v_voice_tick(H7vModule* m, H7vSong* s, H7vVoice* v) {
    run_fseq(m, v);
    run_sweep(m, v);
    run_vseq(v);

    uint16_t period = note_period(v);
    period = hip_vibrato_apply(&v->vibrato, period);
    if (v->info & INFO_PORTAMENTO) {
        period = hip_portamento_apply(&v->porta, v->info_prev, period);
    }
    v->paula.period = period;
    const uint8_t volume = voice_volume(s, v);
    v->paula.volume = v->mixed_slot < 0 ? (uint8_t)(volume & PAULA_VOLUME_MASK) : volume;
}
