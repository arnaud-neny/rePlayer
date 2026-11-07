// SPDX-License-Identifier: MIT
// Jochen Hippel COSO player, translated from the replayer hunk of UADE's "hipc player module V0.2".
//
// HipModule: "COSO", seven offsets (frequency sequences, volume sequences, patterns, tracks, subsongs, sample table,
// sample data), then Hippel's "TFMX" header. Each voice runs its own track (3 bytes per voice per position) and
// pattern speed; notes start a volume sequence (the instrument), which names a frequency sequence. The frequency
// sequence picks samples and sets the note offset; the volume sequence the volume; on top come vibrato, portamento
// and a sample-window sweep.

#include <nuade/players/hippel.h>

#include "hippel_effects.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 0x20
#define TRACK_ROW_SIZE 12 // one position: 3 bytes for each voice
#define SUBSONG_SIZE 6
#define SUBSONG_DEFAULT_SPEED 4
#define SAMPLE_SIZE 10
#define SAMPLE_PACK_RECORD 0x18
#define VSEQ_HEADER 5

// HipSequence bytes from 0xE0 up are commands
#define SEQ_COMMAND 0xE0

// The Wanted Team player's module checks: the first byte of a frequency sequence that loads a sample, the most subsongs
// its tables can address (a word index of 6 bytes per subsong), and the one module it repairs (a long in the sample
// data that it clears)
#define WT_SAMPLE_COMMAND 0xE2
#define WT_MAX_SUBSONGS 5461
#define WT_FIXED_SAMPLE_DATA 0x1646
#define WT_FIXED_OFFSET 0x15CC
#define WT_FIXED_VALUE 0x00FF00FF

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
    FseqCommand_RandomVolume,  // EA depth: (Wanted Team) cut the next tick's volume percent by a random amount up to
                               // depth
} FseqCommand;

typedef enum VseqCommand {
    VseqCommand_Jump = 0,      // E0 pos (counted from the sequence header)
    VseqCommand_Wait = 8,      // E8 ticks; E1..E7 hold
    VseqCommand_SkipFirst = 9, // E9..EF (Wanted Team): skip the command and its argument
    VseqCommand_SkipLast = 15,
    VseqCommand_CrashFirst = 16, // F0..FF (Wanted Team): past the jump table; most run undefined code
} VseqCommand;

// F1, F6 and FC jump into the period computation instead: the base period is the word 0x38 bytes past the byte after
// the command
#define VSEQ_PERIOD_WORD_OFFSET 0x38

// Track byte 2 with the top bit set: a command in the high nibble
#define TRACK_SET_VOLUME 0xF
#define TRACK_STOP 0x8
#define TRACK_SET_SPEED 0xE // (Wanted Team) low nibble: the new speed of every voice

// Pattern bytes
#define PATTERN_END 0xFF
#define PATTERN_SPEED 0xFE
#define PATTERN_SPEED_WAIT 0xFD
#define INFO_HAS_PARAM 0xE0
#define INFO_PORTAMENTO 0x20
#define INFO_ALT_FSEQ 0x40
#define INFO_INSTRUMENT 0x1F
#define NO_FSEQ 0x80

// A voice's pattern position before its first track entry is read (the replayer points it at an end marker)
#define PATTERN_NONE UINT32_MAX

// The sequence every voice starts with (volume and frequency): value 1, then hold.
static const uint8_t k_start_sequence[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE1 };

// AUDxLC before a voice plays anything: one silent word.
static const int8_t k_silence[2] = { 0, 0 };

static const HipVariant k_uade_variant = { 0 };
static const HipVariant k_wanted_team_variant = {
    .wanted_team_module = true,
    .song_end_on_first_voice = true,
    .subsong_fixups = true,
    .track_speed = true,
    .extended_sequences = true,
};

static uint8_t mod_byte(const HipModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t mod_word(const HipModule* m, uint32_t offset) {
    return (uint16_t)(mod_byte(m, offset) << 8 | mod_byte(m, offset + 1));
}

static uint32_t mod_long(const HipModule* m, uint32_t offset) {
    return (uint32_t)mod_word(m, offset) << 16 | mod_word(m, offset + 2);
}

// Table entries are words, added to the module base sign-extended (movea.w).

static uint32_t table_entry(const HipModule* m, uint32_t table, uint32_t index) {
    return (uint32_t)(int32_t)(int16_t)mod_word(m, table + index * 2);
}

static HipSequence module_sequence(const HipModule* m, uint32_t offset) {
    return offset < m->size ? (HipSequence) { m->data + offset, m->size - offset } : (HipSequence) { nullptr, 0 };
}

static uint8_t seq_byte(HipSequence s, uint32_t pos) {
    return pos < s.size ? s.data[pos] : 0;
}

// AUDxLC/AUDxLEN for a sample at a module offset. A sample that starts past the module plays one silent word (the
// original reads whatever memory is there); one that runs past its end plays the zeros behind the padded copy.
typedef struct SampleRange {
    const int8_t* lc;
    uint16_t len_words;
} SampleRange;

static SampleRange sample_range(const HipModule* m, uint32_t at, uint16_t len_words) {
    if (at <= m->size) {
        return (SampleRange) { .lc = (const int8_t*)m->data + at, .len_words = len_words };
    }
    return (SampleRange) { .lc = k_silence, .len_words = sizeof(k_silence) / 2 };
}

// Paula

static void dma_off(HipSong* s, HipVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    s->dma_mask |= v->dma_bit;
}

// Frequency sequence commands

typedef struct SampleEntry {
    uint32_t offset;      // into the sample data
    uint16_t length;      // words
    uint16_t loop_start;  // bytes
    uint16_t loop_length; // words
} SampleEntry;

static SampleEntry sample_entry(const HipModule* m, uint8_t number) {
    const uint32_t at = m->samples + (uint32_t)number * SAMPLE_SIZE;
    return (SampleEntry) { .offset = mod_long(m, at),
                           .length = mod_word(m, at + 4),
                           .loop_start = mod_word(m, at + 6),
                           .loop_length = mod_word(m, at + 8) };
}

// Point the channel at a sample (AUDxLC/AUDxLEN) and set up its repeat.

static void load_sample(const HipModule* m, HipVoice* v, uint8_t number) {
    const SampleEntry e = sample_entry(m, number);
    const SampleRange start = sample_range(m, m->sample_data + e.offset, e.length);
    v->paula.lc = start.lc;
    v->paula.len_words = start.len_words;
    const SampleRange loop = sample_range(m, m->sample_data + e.offset + e.loop_start, e.loop_length);
    v->loop = loop.lc;
    v->loop_length = loop.len_words;
}

static void restart_volume(HipVoice* v) {
    v->vseq_pos = 0;
    v->vseq_counter = 1;
}

// One sample of a sample pack: a block with a record count, an extra size (longs), records of start/end offsets, then
// the data. The replayer copies the first byte of the sample over the second.

static void load_packed_sample(HipModule* m, HipVoice* v, uint8_t number, uint8_t record) {
    const uint32_t block = m->sample_data + sample_entry(m, number).offset;
    const uint32_t count = mod_word(m, block + 4);
    const uint32_t extra = (uint32_t)mod_word(m, block + 6) << 2;
    const uint32_t records = block + 8;
    const uint32_t data = records + count * SAMPLE_PACK_RECORD + extra;
    const uint32_t start = mod_long(m, records + (uint32_t)record * SAMPLE_PACK_RECORD) & ~1u;
    const uint32_t end = mod_long(m, records + (uint32_t)record * SAMPLE_PACK_RECORD + 4) & ~1u;
    const uint32_t at = data + start;
    if (at < m->size - 1) {
        m->data[at + 1] = m->data[at];
    }
    const SampleRange sample = sample_range(m, at, (uint16_t)((end - start) >> 1));
    v->paula.lc = sample.lc;
    v->paula.len_words = sample.len_words;
    v->loop = sample_range(m, at, 1).lc;
    v->loop_length = 1;
}

static uint16_t seq_word(HipSequence s, uint32_t pos) {
    return (uint16_t)(seq_byte(s, pos) << 8 | seq_byte(s, pos + 1));
}

// A frequency sequence byte is a command from E0 up; the Wanted Team replayer has commands up to EA only.

static bool fseq_is_value(const HipVariant* variant, uint8_t c) {
    return c < SEQ_COMMAND || (variant->extended_sequences && c - SEQ_COMMAND > FseqCommand_RandomVolume);
}

// Read the frequency sequence until it yields a value or holds.

static void run_fseq(HipPlayer* p, HipVoice* v) {
    HipModule* m = &p->module;
    HipSong* s = &p->song;

    if (v->fseq_wait != 0) {
        v->fseq_wait--;
        return;
    }
    for (int guard = 0; guard < 256; guard++) {
        const uint32_t at = v->fseq_pos;
        const uint8_t c = seq_byte(v->fseq, at);
        if (fseq_is_value(m->variant, c)) {
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
                v->sample_number = 0xFF;
                v->random_volume = 0;
                dma_off(s, v);
                load_sample(m, v, arg);
                v->paula.period = 4;
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
                v->random_volume = 0;
                dma_off(s, v);
                v->paula.period = 4;
                const SampleEntry e = sample_entry(m, arg);
                v->sweep_start = e.offset;
                v->sweep_end = e.offset + (uint32_t)e.length * 2;
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
                    v->sample_number = arg;
                    v->random_volume = 0;
                    dma_off(s, v);
                    load_sample(m, v, arg);
                    v->paula.period = 4;
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
                v->sample_number = 0xFF;
                v->random_volume = 0;
                dma_off(s, v);
                load_packed_sample(m, v, arg, seq_byte(v->fseq, at + 2));
                v->paula.period = 4;
                restart_volume(v);
                v->fseq_pos += 3;
                v->sweep_active = false;
                break;
            case FseqCommand_RandomVolume:
                if (!m->variant->extended_sequences) {
                    return;
                }
                v->random_volume = arg;
                v->fseq_pos += 2;
                break;
            default:
                return; // the eagleplayer reports a bad command; the replayer would jump into the weeds
        }
    }
}

// Move the sample window: AUDxLC/AUDxLEN become the repeat, no restart.

static void run_sweep(const HipModule* m, HipVoice* v) {
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
        } else if (v->sweep_start + (uint32_t)pos * 2 + (uint32_t)v->sweep_length * 2 > v->sweep_end) {
            v->sweep_stopped = true;
            pos -= v->sweep_delta;
        }
    }
    v->sweep_pos = (uint16_t)pos;
    const SampleRange window
        = sample_range(m, m->sample_data + v->sweep_start + (uint32_t)(uint16_t)pos * 2, v->sweep_length);
    v->loop_length = window.len_words;
    v->loop = window.lc;
    v->paula.lc = window.lc;
    v->paula.len_words = window.len_words;
}

static void run_vseq(const HipVariant* variant, HipSong* s, HipVoice* v) {
    if (v->vseq_wait != 0) {
        v->vseq_wait--;
        return;
    }
    v->vseq_counter--;
    if (v->vseq_counter != 0) {
        return;
    }
    v->vseq_counter = v->vseq_speed;
    for (int guard = 0; guard < 256; guard++) {
        const uint8_t c = seq_byte(v->vseq, v->vseq_pos);
        if (c < SEQ_COMMAND) {
            v->volume = c;
            v->vseq_pos++;
            return;
        }
        const uint8_t arg = seq_byte(v->vseq, v->vseq_pos + 1);
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
        } else if (variant->extended_sequences && c - SEQ_COMMAND >= VseqCommand_SkipFirst
                   && c - SEQ_COMMAND <= VseqCommand_SkipLast) {
            v->vseq_pos += 2;
        } else if (variant->extended_sequences && c - SEQ_COMMAND >= VseqCommand_CrashFirst) {
            if (c == 0xF1 || c == 0xF6 || c == 0xFC) {
                v->seq_period = true;
                v->seq_period_word = seq_word(v->vseq, v->vseq_pos + 1 + VSEQ_PERIOD_WORD_OFFSET);
            } else {
                s->crashed = true;
            }
            return;
        } else {
            return; // E1..E7: hold
        }
    }
}

static uint16_t note_period(const HipVoice* v) {
    uint8_t note = v->fseq_value;
    if (!(note & 0x80)) {
        note = (uint8_t)(note + v->note + v->note_transpose);
    }
    note &= 0x7F;
    return hip_note_period(note);
}

static uint16_t apply_portamento(HipVoice* v, uint16_t period) {
    return (v->info & INFO_PORTAMENTO) ? hip_portamento_apply(&v->porta, v->param, period) : period;
}

// The Wanted Team replayer's random number: a 16-bit generator, one step per use.

static uint8_t next_random(HipSong* s) {
    const uint16_t sum = (uint16_t)(s->random_seed + 0x4793);
    const uint16_t rotated = (uint16_t)(sum >> 6 | sum << 10);
    s->random_seed = rotated ^ sum;
    return (uint8_t)s->random_seed;
}

// The voice volume in percent: the track's level, less a random share of the depth the last EA set (used up by it).

static uint8_t voice_percent(HipSong* s, HipVoice* v) {
    int percent = v->channel_volume;
    if (v->random_volume != 0) {
        percent -= v->random_volume * next_random(s) / 0xFF;
        v->random_volume = 0;
    }
    return (uint8_t)(percent > 0 ? percent : 0);
}

// One voice's tick: sequences and effects. Returns AUDxPER and AUDxVOL.

static void run_voice(HipPlayer* p, HipVoice* v) {
    run_fseq(p, v);
    run_sweep(&p->module, v);
    v->seq_period = false;
    run_vseq(p->module.variant, &p->song, v);

    uint16_t period = v->seq_period ? v->seq_period_word : note_period(v);
    period = hip_vibrato_apply(&v->vibrato, period);
    period = apply_portamento(v, period);
    v->period = period;

    v->paula.period = period;
    // The replayer writes the quotient word to AUDxVOL as it is (move.l period:volume to AUDxPER/AUDxVOL), up to 223
    v->paula.volume = hip_scaled_volume(v->volume, voice_percent(&p->song, v));
}

// Sequencer

static void read_track(HipPlayer* p, HipVoice* v) {
    const HipModule* m = &p->module;
    HipSong* s = &p->song;

    uint32_t at = v->track + v->track_offset;
    if (at == v->track_end) {
        v->track_offset = 0;
        at = v->track;
        s->looped = s->looped || v->index == 0 || !m->variant->song_end_on_first_voice;
    }
    const uint8_t pattern = mod_byte(m, at);
    v->note_transpose = mod_byte(m, at + 1);
    const uint8_t command = mod_byte(m, at + 2);
    if (command <= 0x7F) {
        v->instrument_transpose = command;
    } else if (command >> 4 == TRACK_SET_VOLUME) {
        const uint8_t level = command & 0x0F;
        v->channel_volume = hip_level_percent(level);
    } else if (command >> 4 == TRACK_STOP) {
        s->stopped = true;
    } else if (command >> 4 == TRACK_SET_SPEED && m->variant->track_speed) {
        s->speed = command & 0x0F;
    }
    v->pattern = table_entry(m, m->patterns, pattern);
    v->track_offset = (uint16_t)(v->track_offset + TRACK_ROW_SIZE);
}

static void start_instrument(HipPlayer* p, HipVoice* v) {
    const HipModule* m = &p->module;
    const uint8_t instrument = (uint8_t)((v->info & INFO_INSTRUMENT) + v->instrument_transpose);
    const uint32_t header = table_entry(m, m->vseqs, instrument);
    v->last_instrument = instrument;
    v->vseq_pos = 0;
    v->vseq_counter = mod_byte(m, header);
    v->vseq_speed = mod_byte(m, header);
    uint8_t fseq = mod_byte(m, header + 1);
    hip_vibrato_start(&v->vibrato, mod_byte(m, header + 2), mod_byte(m, header + 3), mod_byte(m, header + 4));
    v->vseq = module_sequence(m, header + VSEQ_HEADER);
    v->vseq_wait = 0;
    if (fseq == NO_FSEQ) {
        return;
    }
    if (v->info & INFO_ALT_FSEQ) {
        fseq = v->param;
    }
    v->fseq = module_sequence(m, table_entry(m, m->fseqs, fseq));
    v->fseq_pos = 0;
    v->fseq_wait = 0;
}

static void step_voice(HipPlayer* p, HipVoice* v) {
    const HipModule* m = &p->module;
    v->row_counter--;
    if (v->row_counter >= 0) {
        return;
    }
    v->row_counter = (int8_t)v->row_speed;
    for (int guard = 0; guard < 256; guard++) {
        uint8_t c = v->pattern != PATTERN_NONE ? mod_byte(m, v->pattern) : PATTERN_END;
        if (c == PATTERN_END) {
            read_track(p, v);
            continue;
        }
        uint32_t at = v->pattern + 1;
        // speed changes read on in the same pattern; they count towards the guard (a damaged pattern of nothing else)
        while (c == PATTERN_SPEED) {
            if (++guard == 256) {
                return;
            }
            v->row_speed = mod_byte(m, at);
            v->row_counter = (int8_t)v->row_speed;
            c = mod_byte(m, at + 1);
            at += 2;
        }
        if (c == PATTERN_SPEED_WAIT) {
            v->row_speed = mod_byte(m, at);
            v->row_counter = (int8_t)v->row_speed;
            v->pattern = at + 1;
            return;
        }
        if (c == PATTERN_END) {
            // a speed change right before the end: the replayer reads the end marker as the next byte too
            read_track(p, v);
            continue;
        }
        v->note = c;
        v->info = mod_byte(m, at++);
        if (v->info & INFO_HAS_PARAM) {
            v->param = mod_byte(m, at++);
        }
        v->pattern = at;
        v->porta = 0;
        if (!(c & 0x80)) {
            start_instrument(p, v);
        }
        return;
    }
}

bool hip_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= HEADER_SIZE + 4 && memcmp(data, "COSO", 4) == 0
           && memcmp(data + HEADER_SIZE, "TFMX", 4) == 0;
}

static void mod_set_word(HipModule* m, uint32_t offset, uint16_t value) {
    if (m->size >= 2 && offset <= m->size - 2) {
        m->data[offset] = (uint8_t)(value >> 8);
        m->data[offset + 1] = (uint8_t)value;
    }
}

// The Wanted Team player's Check2. A module that passes has its samples behind its sample data offset. Any other one
// (the Atari ST modules) takes the sample set file "SMP.set"; UADE's harness plays those on after the failed check,
// reading memory the port does not have, so the port does not take them.

static bool wt_check2(const HipModule* m) {
    if (mod_word(m, 0x30) == 0 || mod_word(m, 0x40) == 0) {
        return false;
    }
    const uint32_t sample_data = mod_long(m, 0x1C);
    if (sample_data == 0 || (int32_t)(m->size - sample_data) <= 0) {
        return false;
    }
    // More frequency sequences start with a sample command with a number below 0x80 than with a higher one
    int plain = 0;
    int high = 0;
    for (uint32_t i = 0; i <= mod_word(m, 0x24); i++) {
        const uint32_t seq = table_entry(m, m->fseqs, i);
        if (mod_byte(m, seq) == WT_SAMPLE_COMMAND) {
            if (mod_byte(m, seq + 1) & 0x80) {
                high++;
            } else {
                plain++;
            }
        }
    }
    if (high >= plain) {
        return false;
    }
    // The first track row must not have 0xFF in the second voice's pattern byte (its offset is a signed word)
    const uint32_t first_row = (uint32_t)(int32_t)(int16_t)m->tracks;
    return mod_byte(m, first_row + 3) != 0xFF;
}

// InitPlayer's size test: the sample data offset plus the end of the last sample (offset + length) must lie within the
// module. The last sample is the one before the header's sample count (a count of 0 reads the bytes in front of the
// sample table).

static bool wt_samples_fit(const HipModule* m) {
    const uint32_t count_bytes = (uint32_t)(int32_t)(int16_t)(uint16_t)(mod_word(m, 0x32) * SAMPLE_SIZE);
    const uint32_t table_end = m->samples + count_bytes;
    const uint32_t last_end = mod_long(m, table_end - SAMPLE_SIZE) + 2u * mod_word(m, table_end - 6);
    return (int32_t)(m->sample_data + last_end) <= (int32_t)m->size;
}

// The UADE player's checks of a module: its subsong range is the number of entries before the sample table, less one.

static bool uade_module_valid(HipModule* m) {
    m->subsong_count = (int)((int16_t)((m->samples - m->subsongs) / SUBSONG_SIZE) - 1);
    return m->subsong_count >= 1 && m->sample_data <= m->size;
}

// The Wanted Team player's checks of a module. Its subsong range is a header field.

static bool wanted_team_module_valid(HipModule* m) {
    if (!wt_check2(m)) {
        return false;
    }
    m->subsong_count = mod_word(m, 0x30) < WT_MAX_SUBSONGS ? mod_word(m, 0x30) : WT_MAX_SUBSONGS;
    // The player's fix for one known module: a long in the sample data
    if (m->sample_data == WT_FIXED_SAMPLE_DATA && mod_long(m, WT_FIXED_OFFSET) == WT_FIXED_VALUE) {
        mod_set_word(m, WT_FIXED_OFFSET, 0);
        mod_set_word(m, WT_FIXED_OFFSET + 2, 0);
    }
    return m->subsong_count >= 1 && wt_samples_fit(m);
}

static HipPlayer* create_player(const HipVariant* variant, const uint8_t* data, size_t size) {
    if (size > INT32_MAX / 2) {
        return nullptr;
    }
    HipPlayer* p = calloc(1, sizeof(HipPlayer));
    uint8_t* copy = nuade_paula_padded_copy(data, size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    HipModule* m = &p->module;
    m->variant = variant;
    m->data = copy;
    m->size = (uint32_t)size;
    m->fseqs = mod_long(m, 0x04);
    m->vseqs = mod_long(m, 0x08);
    m->patterns = mod_long(m, 0x0C);
    m->tracks = mod_long(m, 0x10);
    m->subsongs = mod_long(m, 0x14);
    m->samples = mod_long(m, 0x18);
    m->sample_data = mod_long(m, 0x1C);
    if (!(variant->wanted_team_module ? wanted_team_module_valid(m) : uade_module_valid(m))) {
        hip_destroy(p);
        return nullptr;
    }
    return p;
}

HipPlayer* hip_create(const uint8_t* data, size_t size) {
    return hip_probe(data, size) ? create_player(&k_uade_variant, data, size) : nullptr;
}

HipPlayer* hip_create_wanted_team(const uint8_t* data, size_t size) {
    return hip_probe(data, size) ? create_player(&k_wanted_team_variant, data, size) : nullptr;
}

void hip_destroy(HipPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
        free(player);
    }
}

int hip_subsong_count(const HipPlayer* player) {
    return player->module.subsong_count;
}

// The Wanted Team InitSound repairs the subsong entry in the module: a speed of 0 becomes 4, and a last position
// before the first (the 16-bit difference is negative) makes both 0.
static void fix_subsong_entry(HipModule* m, uint32_t entry) {
    if (mod_word(m, entry + 4) == 0) {
        mod_set_word(m, entry + 4, SUBSONG_DEFAULT_SPEED);
    }
    if ((int16_t)(mod_word(m, entry + 2) - mod_word(m, entry)) < 0) {
        mod_set_word(m, entry, 0);
        mod_set_word(m, entry + 2, 0);
    }
}

// The replayer's init with subsong n (1-based): subsong entry n - 1 gives the first and last position and the speed.
bool hip_start(HipPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= player->module.subsong_count) {
        return false;
    }
    HipModule* m = &player->module;
    HipSong* s = &player->song;
    *s = (HipSong) { 0 };

    const uint32_t entry = m->subsongs + (uint32_t)subsong * SUBSONG_SIZE;
    if (m->variant->subsong_fixups) {
        fix_subsong_entry(m, entry);
    }
    const uint32_t first = mod_word(m, entry);
    const uint32_t last = mod_word(m, entry + 2);
    s->speed = mod_word(m, entry + 4);
    s->speed_counter = 1;

    for (int i = 0; i < HIP_VOICES; i++) {
        HipVoice* v = &s->voices[i];
        const uint32_t column = (uint32_t)i * 3;
        v->index = i;
        v->dma_bit = (uint16_t)(1u << i);
        v->fseq = (HipSequence) { k_start_sequence, sizeof(k_start_sequence) };
        v->vseq = v->fseq;
        v->vseq_counter = 1;
        v->vseq_speed = 1;
        v->channel_volume = HIP_VOLUME_FULL;
        v->sample_number = 0xFF;
        v->row_counter = -1;
        v->track = m->tracks + first * TRACK_ROW_SIZE + column;
        v->track_end = m->tracks + (last + 1) * TRACK_ROW_SIZE + column;
        v->pattern = PATTERN_NONE;
        v->last_instrument = -1;
        v->paula.lc = k_silence;
        v->paula.len_words = 1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

HipTickResult hip_tick(HipPlayer* player, NuadeVoices* voices) {
    HipSong* s = &player->song;
    s->looped = false;
    if (s->stopped) {
        for (int i = 0; i < HIP_VOICES; i++) {
            HipVoice* v = &s->voices[i];
            nuade_paula_channel_dma_off(&v->paula);
            v->paula.period = 0;
            v->paula.volume = 0;
            nuade_paula_channel_sync(&v->paula, (uint32_t)i, voices);
        }
        return (HipTickResult) { .ended = true };
    }

    s->dma_mask = 0;
    for (int i = 0; i < HIP_VOICES && !s->crashed; i++) {
        run_voice(player, &s->voices[i]);
    }
    if (s->crashed) {
        return (HipTickResult) { .ended = true };
    }
    if (s->dma_mask != 0) {
        // DMA on, a raster line, then the repeat registers of every voice
        for (int i = 0; i < HIP_VOICES; i++) {
            HipVoice* v = &s->voices[i];
            if (s->dma_mask & v->dma_bit) {
                nuade_paula_channel_dma_on(&v->paula);
            }
        }
        for (int i = 0; i < HIP_VOICES; i++) {
            HipVoice* v = &s->voices[i];
            v->paula.lc = v->loop;
            v->paula.len_words = v->loop_length;
        }
    }

    s->speed_counter--;
    if (s->speed_counter == 0) {
        s->speed_counter = s->speed;
        for (int i = 0; i < HIP_VOICES; i++) {
            step_voice(player, &s->voices[i]);
        }
    }

    for (int i = 0; i < HIP_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (HipTickResult) { .looped = s->looped };
}

int hip_speed(const HipPlayer* player) {
    return player->song.speed;
}

HipVoiceState hip_voice_state(const HipPlayer* player, int voice) {
    const HipVoice* v = &player->song.voices[voice];
    const bool has_note = v->last_instrument >= 0 && !(v->note & 0x80);
    return (HipVoiceState) { .note = has_note ? (v->note + v->note_transpose) & 0x7F : -1,
                             .instrument = v->last_instrument };
}
