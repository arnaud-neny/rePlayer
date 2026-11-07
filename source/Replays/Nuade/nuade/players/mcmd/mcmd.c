// SPDX-License-Identifier: MIT
// MCMD player, translated from the replayer hunk of UADE's "MCMD player module V1.0 (15 Feb 93)".
//
// The engine is the one of Jochen Hippel's COSO player (a track of three-byte entries per voice, patterns, volume
// sequences with a five byte header, frequency sequences, vibrato, a voice volume in percent) in a module whose
// sequences are 64 bytes each and whose patterns have one fixed size. McmdModule: "MCMD", seven counts, then the
// tables: frequency sequences, volume sequences, patterns, track rows (12 bytes: three for each voice), subsong entries
// (8 bytes), samples (28 bytes) and the sample data.

#include <nuade/players/mcmd.h>

#include "../hippel/hippel_effects.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 0x12
#define SEQUENCE_SIZE 64 // a frequency or volume sequence
#define TRACK_ROW_SIZE 12
#define SUBSONG_SIZE 8
#define SAMPLE_SIZE 28
#define VSEQ_HEADER 5
#define ROW_SIZE 2 // a pattern row: note, info

// McmdSequence bytes from 0xE0 up are commands
#define SEQ_COMMAND 0xE0

// The replayer adds table sizes to its pointers with adda.w: a size of 0x8000 or more would go backwards, and the port
// only takes modules where no size does.
#define MAX_TABLE_SIZE 0x7FFF

// A note with this value (top bit ignored) ends the pattern
#define NOTE_PATTERN_END 1

// Info byte: instrument in bits 0-5, portamento (6), alternative frequency sequence (7); both take the parameter in
// the info byte of the row before
#define INFO_INSTRUMENT 0x3F
#define INFO_PORTAMENTO 0x40
#define INFO_ALT_FSEQ 0x80
#define NOTE_NO_INSTRUMENT 0x80

// Track byte 2: up to 0x7F the instrument transpose, with the top bit set a command in the high nibble
#define TRACK_SET_VOLUME 0xF
#define TRACK_STOP 0x8

// The portamento adds its speed shifted up by this many bits to a 32-bit accumulator whose high word moves the period
#define PORTAMENTO_SHIFT 11

// The period table is the one of the COSO player with one more entry; what follows it in the replayer's memory is 0
#define PERIOD_AFTER_TABLE 598

typedef enum FseqCommand {
    FseqCommand_Jump = 0,      // E0 pos
    FseqCommand_Hold,          // E1
    FseqCommand_Sample,        // E2 sample: start it, restart the volume sequence
    FseqCommand_Vibrato,       // E3 speed depth
    FseqCommand_SampleNoReset, // E4 sample: switch the playing sample at its next loop
    FseqCommand_SampleIfNew,   // E5 sample: start it unless it is the current one, restart the volume sequence
    FseqCommand_Skip,          // E6: the command byte only
    FseqCommand_Sequence,      // E7 sequence: continue in another frequency sequence
    FseqCommand_Wait,          // E8 ticks
    FseqCommand_Last = FseqCommand_Wait, // the jump table ends here; a larger command lands in the code behind it
} FseqCommand;

typedef enum VseqCommand {
    VseqCommand_Jump = 0, // E0 pos (counted from the sequence header)
    VseqCommand_Wait = 8, // E8 ticks; E1..E7 hold
    VseqCommand_Last = VseqCommand_Wait,
} VseqCommand;

// Commands behind the jump tables whose table word lands on code that goes on: EE of a frequency sequence moves the
// read pointer 12 bytes on; ED, F2 and F7 of a volume sequence enter the period computation with note 38 (the table
// word 0x26) and leave the sequence position where it is. Every other command behind a table runs off into the weeds.
#define FSEQ_SKIP_COMMAND 0xEE
#define FSEQ_SKIP_BYTES 12
#define VSEQ_NOTE_COMMAND_FIRST 0xED
#define VSEQ_NOTE_COMMAND_SECOND 0xF2
#define VSEQ_NOTE_COMMAND_THIRD 0xF7
#define VSEQ_NOTE_PERIOD_INDEX 38

// sample_number after E2, and from where on the sequencer stops the channel before a new note: the replayer tests the
// number's sign
#define SAMPLE_FORCED 0xFF
#define SAMPLE_FORCED_FIRST 0x80

#define FSEQ_ABSOLUTE_NOTE 0x80
#define NOTE_MASK 0x7F

// After the last track row the replayer reads the next row from the song end routine instead: these are the first bytes
// of UADE's routine (the replayer leaves its address in the register it reads the row from).
static const uint8_t k_song_end_row[3] = { 0x2F, 0x08, 0x41 };

// The sequence every voice starts with (volume and frequency): value 1, then hold.
static const uint8_t k_start_sequence[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE1 };

// AUDxLC before a voice plays anything: one silent word.
static const int8_t k_silence[2] = { 0, 0 };

// The note and info byte of each voice record in the replayer's data block, which the init does not set. They give the
// periods of a voice that has not been given a note yet.
static const struct {
    uint8_t note;
    uint8_t info;
} k_initial_row[MCMD_VOICES] = { { 0x4B, 0 }, { 0, 0 }, { 0x0C, 1 }, { 0, 0 } };

// A track row's entry for one voice
typedef struct TrackEntry {
    uint8_t pattern;
    uint8_t note_transpose;
    uint8_t command; // the instrument transpose up to 0x7F, else a command in the high nibble
} TrackEntry;

typedef struct SampleEntry {
    uint32_t offset;      // into the sample data
    uint16_t length;      // words
    uint16_t loop_start;  // bytes
    uint16_t loop_length; // words
} SampleEntry;

static uint8_t mod_byte(const McmdModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t mod_word(const McmdModule* m, uint32_t offset) {
    return (uint16_t)(mod_byte(m, offset) << 8 | mod_byte(m, offset + 1));
}

static uint32_t mod_long(const McmdModule* m, uint32_t offset) {
    return (uint32_t)mod_word(m, offset) << 16 | mod_word(m, offset + 2);
}

// A word offset added to a pointer sign-extended (adda.w)

static uint32_t add_word(uint32_t base, uint16_t offset) {
    return base + (uint32_t)(int32_t)(int16_t)offset;
}

static McmdSequence module_sequence(const McmdModule* m, uint32_t offset) {
    return (McmdSequence) { .data = m->data, .size = m->size, .origin = offset };
}

static uint8_t seq_byte(McmdSequence s, uint32_t at) {
    const uint32_t address = s.origin + at;
    return address < s.size ? s.data[address] : 0;
}

// The read pointer the replayer builds from a position word

static uint32_t seq_address(uint16_t pos) {
    return add_word(0, pos);
}

// AUDxLC/AUDxLEN for a sample at a module offset. A sample that starts past the module plays one silent word (the
// original reads whatever memory is there); one that runs past its end plays the zeros behind the padded copy.
typedef struct SampleRange {
    const int8_t* lc;
    uint16_t len_words;
} SampleRange;

static SampleRange sample_range(const McmdModule* m, uint32_t at, uint16_t len_words) {
    if (at <= m->size) {
        return (SampleRange) { .lc = (const int8_t*)m->data + at, .len_words = len_words };
    }
    return (SampleRange) { .lc = k_silence, .len_words = sizeof(k_silence) / 2 };
}

// The header

typedef struct Header {
    uint16_t fseq_count;
    uint16_t vseq_count;
    uint16_t pattern_count;
    uint16_t track_rows;
    uint16_t pattern_size;
    uint16_t subsong_count;
    uint16_t sample_count;
} Header;

static Header read_header(const uint8_t* data) {
    return (Header) { .fseq_count = nuade_read_be16(data + 4),
                      .vseq_count = nuade_read_be16(data + 6),
                      .pattern_count = nuade_read_be16(data + 8),
                      .track_rows = nuade_read_be16(data + 10),
                      .pattern_size = nuade_read_be16(data + 12),
                      .subsong_count = nuade_read_be16(data + 14),
                      .sample_count = nuade_read_be16(data + 16) };
}

static McmdSubsong read_subsong(const McmdModule* m) {
    return (McmdSubsong) { .first = mod_word(m, m->subsongs),
                           .last = mod_word(m, m->subsongs + 2),
                           .speed = mod_word(m, m->subsongs + 4),
                           .loop = mod_word(m, m->subsongs + 6) };
}

// The table offsets the replayer computes, in the order it adds them. The module is accepted when no table size
// reaches the sign bit of the word the replayer adds it with, the tables lie inside the module and the first subsong
// is a range of track rows it can walk. The replayer takes anything with the magic and reads whatever memory its
// pointers reach; the port takes the modules that are in order.

static bool layout_module(McmdModule* m, const uint8_t* data, size_t size) {
    if (data == nullptr || size < HEADER_SIZE || size > INT32_MAX / 2 || memcmp(data, "MCMD", 4) != 0) {
        return false;
    }
    const Header h = read_header(data);
    m->data = (uint8_t*)data; // only read here
    m->size = (uint32_t)size;
    const uint64_t fseq_bytes = (uint64_t)h.fseq_count * SEQUENCE_SIZE;
    const uint64_t vseq_bytes = (uint64_t)h.vseq_count * SEQUENCE_SIZE;
    const uint64_t pattern_bytes = (uint64_t)h.pattern_count * h.pattern_size;
    const uint64_t track_bytes = (uint64_t)h.track_rows * TRACK_ROW_SIZE;
    const uint64_t subsong_bytes = (uint64_t)h.subsong_count * SUBSONG_SIZE;
    const uint64_t sample_bytes = (uint64_t)h.sample_count * SAMPLE_SIZE;
    if (fseq_bytes > MAX_TABLE_SIZE || vseq_bytes > MAX_TABLE_SIZE || pattern_bytes > MAX_TABLE_SIZE
        || track_bytes > MAX_TABLE_SIZE || subsong_bytes > MAX_TABLE_SIZE || sample_bytes > MAX_TABLE_SIZE) {
        return false;
    }
    // A pattern ends when its position equals its size, stepping by whole rows; the replayer adds the size to a pointer
    // as a signed word whatever the pattern count is
    if (h.pattern_size == 0 || h.pattern_size > MAX_TABLE_SIZE || h.pattern_size % ROW_SIZE != 0
        || h.subsong_count == 0) {
        return false;
    }
    m->fseqs = HEADER_SIZE;
    m->vseqs = m->fseqs + (uint32_t)fseq_bytes;
    m->patterns = m->vseqs + (uint32_t)vseq_bytes;
    m->pattern_size = h.pattern_size;
    m->tracks = m->patterns + (uint32_t)pattern_bytes;
    m->subsongs = m->tracks + (uint32_t)track_bytes;
    m->samples = m->subsongs + (uint32_t)subsong_bytes;
    m->sample_data = m->samples + (uint32_t)sample_bytes;
    if (m->sample_data > size) {
        return false;
    }
    // The first subsong: track rows first..last, restarting at a row inside them. The track position is a signed word.
    const McmdSubsong subsong = read_subsong(m);
    return subsong.first <= subsong.loop && subsong.loop <= subsong.last && subsong.last < h.track_rows
           && (subsong.last + 1u) * TRACK_ROW_SIZE <= MAX_TABLE_SIZE;
}

bool mcmd_probe(const uint8_t* data, size_t size) {
    McmdModule m = { 0 };
    return layout_module(&m, data, size);
}

// Paula

static void dma_off(McmdVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    v->dma_restart = true;
}

static SampleEntry sample_entry(const McmdModule* m, uint8_t number) {
    const uint32_t at = m->samples + (uint32_t)number * SAMPLE_SIZE;
    return (SampleEntry) { .offset = mod_long(m, at + 0x12),
                           .length = mod_word(m, at + 0x16),
                           .loop_start = mod_word(m, at + 0x18),
                           .loop_length = mod_word(m, at + 0x1A) };
}

// Point the channel at a sample (AUDxLC/AUDxLEN) and set up its repeat.

static void load_sample(const McmdModule* m, McmdVoice* v, uint8_t number) {
    const SampleEntry e = sample_entry(m, number);
    const SampleRange start = sample_range(m, m->sample_data + e.offset, e.length);
    v->paula.lc = start.lc;
    v->paula.len_words = start.len_words;
    const SampleRange loop = sample_range(m, m->sample_data + e.offset + e.loop_start, e.loop_length);
    v->loop = loop.lc;
    v->loop_length = loop.len_words;
}

static void restart_volume(McmdVoice* v) {
    v->vseq_pos = 0;
    v->vseq_counter = 1;
}

// A new sample with the channel's DMA stopped; the tick starts it again

static void restart_sample(const McmdModule* m, McmdVoice* v, uint8_t number) {
    dma_off(v);
    load_sample(m, v, number);
}

// Read the frequency sequence until it yields a value or holds. A command behind the jump table lands in the code
// behind it (see FSEQ_SKIP_COMMAND).

static void run_fseq(McmdPlayer* p, McmdVoice* v) {
    const McmdModule* m = &p->module;
    McmdSong* s = &p->song;

    if (v->fseq_wait != 0) {
        v->fseq_wait--;
        return;
    }
    // `at` is the replayer's read pointer, loaded from fseq_pos and stepped over every byte it reads; fseq_pos only
    // moves by what each command adds to it
    uint32_t at = seq_address(v->fseq_pos);
    for (int guard = 0; guard < 256; guard++) {
        const uint8_t c = seq_byte(v->fseq, at++);
        if (c < SEQ_COMMAND) {
            v->fseq_value = c;
            v->fseq_pos++;
            return;
        }
        if (c == FSEQ_SKIP_COMMAND) {
            at += FSEQ_SKIP_BYTES;
            continue;
        }
        if (c - SEQ_COMMAND > FseqCommand_Last) {
            s->crashed = true;
            return;
        }
        switch ((FseqCommand)(c - SEQ_COMMAND)) {
            case FseqCommand_Jump:
                v->fseq_pos = seq_byte(v->fseq, at);
                at = v->fseq_pos;
                break;
            case FseqCommand_Hold:
                return;
            case FseqCommand_Sample:
                v->sample_number = SAMPLE_FORCED;
                restart_sample(m, v, seq_byte(v->fseq, at++));
                restart_volume(v);
                v->fseq_pos += 2;
                break;
            case FseqCommand_Vibrato:
                v->vibrato.speed = seq_byte(v->fseq, at++);
                v->vibrato.depth = seq_byte(v->fseq, at++);
                v->fseq_pos += 3;
                break;
            case FseqCommand_SampleNoReset:
                load_sample(m, v, seq_byte(v->fseq, at++));
                v->fseq_pos += 2;
                break;
            case FseqCommand_SampleIfNew: {
                const uint8_t number = seq_byte(v->fseq, at++);
                if (number != v->sample_number) {
                    v->sample_number = number;
                    restart_sample(m, v, number);
                }
                restart_volume(v);
                v->fseq_pos += 2;
                break;
            }
            case FseqCommand_Skip:
                break;
            case FseqCommand_Sequence:
                v->fseq = module_sequence(m, add_word(m->fseqs, (uint16_t)(seq_byte(v->fseq, at) << 6)));
                v->fseq_pos = 0;
                at = 0;
                break;
            case FseqCommand_Wait:
                v->fseq_wait = seq_byte(v->fseq, at);
                v->fseq_pos += 2;
                at = seq_address(v->fseq_pos);
                // back to the top, where the wait is counted down
                if (v->fseq_wait != 0) {
                    v->fseq_wait--;
                    return;
                }
                break;
        }
    }
}

static bool is_vseq_note_command(uint8_t c) {
    return c == VSEQ_NOTE_COMMAND_FIRST || c == VSEQ_NOTE_COMMAND_SECOND || c == VSEQ_NOTE_COMMAND_THIRD;
}

static void run_vseq(McmdSong* s, McmdVoice* v) {
    if (v->vseq_wait != 0) {
        v->vseq_wait--;
        return;
    }
    v->vseq_counter--;
    if (v->vseq_counter != 0) {
        return;
    }
    v->vseq_counter = v->vseq_speed;
    uint32_t at = seq_address(v->vseq_pos);
    for (int guard = 0; guard < 256; guard++) {
        const uint8_t c = seq_byte(v->vseq, at++);
        if (c < SEQ_COMMAND) {
            v->volume = c;
            v->vseq_pos++;
            return;
        }
        if (is_vseq_note_command(c)) {
            v->vseq_period = true;
            return;
        }
        if (c - SEQ_COMMAND > VseqCommand_Last) {
            s->crashed = true;
            return;
        }
        if (c - SEQ_COMMAND == VseqCommand_Jump) {
            v->vseq_pos = (uint16_t)(seq_byte(v->vseq, at) - VSEQ_HEADER);
            at = seq_address(v->vseq_pos);
        } else if (c - SEQ_COMMAND == VseqCommand_Wait) {
            v->vseq_wait = seq_byte(v->vseq, at);
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
            at = seq_address(v->vseq_pos);
        } else {
            return; // E1..E7: hold
        }
    }
}

static uint16_t period_at(uint8_t index) {
    return index == HIP_PERIOD_COUNT ? PERIOD_AFTER_TABLE : hip_note_period(index);
}

static uint16_t note_period(const McmdVoice* v) {
    uint8_t note = v->fseq_value;
    if (!(note & FSEQ_ABSOLUTE_NOTE)) {
        note = (uint8_t)(note + v->note + v->note_transpose);
    }
    return period_at(note & NOTE_MASK);
}

// The parameter byte is the speed, negative for a slide down; the period moves by the high word of the accumulator.

static uint16_t apply_portamento(McmdVoice* v, uint16_t period) {
    if (!(v->info & INFO_PORTAMENTO)) {
        return period;
    }
    const uint8_t speed = v->param;
    if (!(speed & 0x80)) {
        v->porta += (uint32_t)speed << PORTAMENTO_SHIFT;
        return (uint16_t)(period - (v->porta >> 16));
    }
    v->porta += (uint32_t)(uint8_t)-speed << PORTAMENTO_SHIFT;
    return (uint16_t)(period + (v->porta >> 16));
}

// One voice's tick: sequences and effects. Sets AUDxPER and AUDxVOL.

static void run_voice(McmdPlayer* p, McmdVoice* v) {
    run_fseq(p, v);
    v->vseq_period = false;
    run_vseq(&p->song, v);

    uint16_t period = v->vseq_period ? hip_note_period(VSEQ_NOTE_PERIOD_INDEX) : note_period(v);
    period = hip_vibrato_apply(&v->vibrato, period);
    v->paula.period = apply_portamento(v, period);
    v->paula.volume = hip_scaled_volume(v->volume, v->channel_volume);
}

// Sequencer

static TrackEntry track_entry(const McmdModule* m, uint32_t at) {
    return (TrackEntry) { .pattern = mod_byte(m, at),
                          .note_transpose = mod_byte(m, at + 1),
                          .command = mod_byte(m, at + 2) };
}

// The voice's next track row entry: its pattern, note transpose, and the instrument transpose or a command.

static void read_track_row(McmdSong* s, McmdVoice* v, const McmdModule* m) {
    const uint32_t at = add_word(v->track, v->track_offset);
    TrackEntry entry;
    if (at == v->track_end) {
        v->track_offset = (uint16_t)((s->subsong.loop - s->subsong.first) * TRACK_ROW_SIZE);
        s->looped = true;
        entry = (TrackEntry) { k_song_end_row[0], k_song_end_row[1], k_song_end_row[2] };
    } else {
        entry = track_entry(m, at);
    }
    v->note_transpose = entry.note_transpose;
    if (entry.command <= NOTE_MASK) {
        v->instrument_transpose = entry.command;
    } else if (entry.command >> 4 == TRACK_SET_VOLUME) {
        v->channel_volume = hip_level_percent(entry.command & 0x0F);
    } else if (entry.command >> 4 == TRACK_STOP) {
        s->stopped = true;
    }
    v->pattern = m->patterns + (uint32_t)entry.pattern * m->pattern_size;
    v->pattern_pos = 0;
    v->track_offset = (uint16_t)(v->track_offset + TRACK_ROW_SIZE);
}

static void start_instrument(const McmdModule* m, McmdVoice* v) {
    const uint8_t instrument = (uint8_t)((v->info & INFO_INSTRUMENT) + v->instrument_transpose);
    const uint32_t header = m->vseqs + ((uint32_t)instrument << 6);
    v->last_instrument = instrument;
    v->vseq_pos = 0;
    v->vseq_counter = mod_byte(m, header);
    v->vseq_speed = mod_byte(m, header);
    uint8_t fseq = mod_byte(m, header + 1);
    hip_vibrato_start(&v->vibrato, mod_byte(m, header + 2), mod_byte(m, header + 3), mod_byte(m, header + 4));
    v->vseq = module_sequence(m, header + VSEQ_HEADER);
    v->vseq_wait = 0;
    if (v->info & INFO_ALT_FSEQ) {
        fseq = v->param;
    }
    v->fseq = module_sequence(m, m->fseqs + ((uint32_t)fseq << 6));
    v->fseq_pos = 0;
    v->fseq_wait = 0;
}

// One row of a voice's pattern. A row whose note is 0 does nothing.

static void step_voice(McmdPlayer* p, McmdVoice* v) {
    const McmdModule* m = &p->module;
    McmdSong* s = &p->song;

    if ((mod_byte(m, add_word(v->pattern, v->pattern_pos)) & NOTE_MASK) == NOTE_PATTERN_END
        || v->pattern_pos == m->pattern_size) {
        read_track_row(s, v, m);
    }
    const uint32_t row = add_word(v->pattern, v->pattern_pos);
    const uint8_t note_byte = mod_byte(m, row);
    const uint16_t row_pos = v->pattern_pos;
    v->pattern_pos = (uint16_t)(v->pattern_pos + ROW_SIZE);
    if ((note_byte & NOTE_MASK) == 0) {
        return;
    }
    v->porta = 0;
    v->note = note_byte & NOTE_MASK;
    v->info = mod_byte(m, row + 1);
    if (v->info & (INFO_PORTAMENTO | INFO_ALT_FSEQ)) {
        // The parameter row is the one before; for the first row of a pattern that is the last one.
        const uint32_t before = row + 1 + (row_pos == 0 ? m->pattern_size : 0) - ROW_SIZE;
        v->param = mod_byte(m, before);
    }
    if (note_byte & NOTE_NO_INSTRUMENT) {
        return;
    }
    // The voice's DMA is switched off when the last sequence started a sample with E2. The frequency sequences' own
    // restart list is cleared right after the sequencer, so it is not kept.
    if (v->sample_number >= SAMPLE_FORCED_FIRST) {
        nuade_paula_channel_dma_off(&v->paula);
    }
    start_instrument(m, v);
}

// Play

static void silence(McmdSong* s, NuadeVoices* voices) {
    for (int i = 0; i < MCMD_VOICES; i++) {
        McmdVoice* v = &s->voices[i];
        nuade_paula_channel_dma_off(&v->paula);
        v->paula.volume = 0;
        nuade_paula_channel_sync(&v->paula, (uint32_t)i, voices);
    }
}

McmdTickResult mcmd_tick(McmdPlayer* player, NuadeVoices* voices) {
    McmdSong* s = &player->song;
    s->looped = false;
    if (s->stopped) {
        silence(s, voices);
        return (McmdTickResult) { .ended = true };
    }

    s->speed_counter--;
    if (s->speed_counter == 0) {
        s->speed_counter = s->subsong.speed;
        for (int i = 0; i < MCMD_VOICES; i++) {
            step_voice(player, &s->voices[i]);
        }
    }

    for (int i = 0; i < MCMD_VOICES; i++) {
        s->voices[i].dma_restart = false;
    }
    for (int i = 0; i < MCMD_VOICES && !s->crashed; i++) {
        run_voice(player, &s->voices[i]);
    }
    if (s->crashed) {
        return (McmdTickResult) { .ended = true };
    }
    bool restarted = false;
    for (int i = 0; i < MCMD_VOICES; i++) {
        restarted = restarted || s->voices[i].dma_restart;
    }
    if (restarted) {
        // DMA on, a raster line, then the repeat registers of every voice
        for (int i = 0; i < MCMD_VOICES; i++) {
            McmdVoice* v = &s->voices[i];
            if (v->dma_restart) {
                nuade_paula_channel_dma_on(&v->paula);
            }
        }
        for (int i = 0; i < MCMD_VOICES; i++) {
            McmdVoice* v = &s->voices[i];
            v->paula.lc = v->loop;
            v->paula.len_words = v->loop_length;
        }
    }
    for (int i = 0; i < MCMD_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (McmdTickResult) { .looped = s->looped };
}

McmdPlayer* mcmd_create(const uint8_t* data, size_t size) {
    McmdModule layout = { 0 };
    if (!layout_module(&layout, data, size)) {
        return nullptr;
    }
    McmdPlayer* p = calloc(1, sizeof(McmdPlayer));
    uint8_t* copy = nuade_paula_padded_copy(data, size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    p->module = layout;
    p->module.data = copy;
    p->module.size = (uint32_t)size;
    return p;
}

void mcmd_destroy(McmdPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
        free(player);
    }
}

// The replayer's init: every voice reads the first track row of the subsong as its first pattern, without decoding the
// command byte, and starts on the start sequences with the sample unset.
bool mcmd_start(McmdPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    McmdModule* m = &player->module;
    McmdSong* s = &player->song;
    *s = (McmdSong) { 0 };
    s->subsong = read_subsong(m);
    s->speed_counter = 1;

    for (int i = 0; i < MCMD_VOICES; i++) {
        McmdVoice* v = &s->voices[i];
        const uint32_t column = (uint32_t)i * 3;
        v->fseq = (McmdSequence) { k_start_sequence, sizeof(k_start_sequence), 0 };
        v->vseq = v->fseq;
        v->note = k_initial_row[i].note;
        v->info = k_initial_row[i].info;
        v->vseq_counter = 1;
        v->vseq_speed = 1;
        v->channel_volume = HIP_VOLUME_FULL;
        v->sample_number = SAMPLE_FORCED;
        v->track = m->tracks + (uint32_t)s->subsong.first * TRACK_ROW_SIZE + column;
        v->track_end = m->tracks + ((uint32_t)s->subsong.last + 1) * TRACK_ROW_SIZE + column;
        v->track_offset = TRACK_ROW_SIZE;
        const TrackEntry first_entry = track_entry(m, v->track);
        v->pattern = m->patterns + (uint32_t)first_entry.pattern * m->pattern_size;
        v->note_transpose = first_entry.note_transpose;
        v->instrument_transpose = first_entry.command;
        v->last_instrument = -1;
        v->paula.lc = k_silence;
        v->paula.len_words = 1;
        v->loop = k_silence;
        v->loop_length = 1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

int mcmd_speed(const McmdPlayer* player) {
    return player->song.subsong.speed;
}

McmdVoiceState mcmd_voice_state(const McmdPlayer* player, int voice) {
    const McmdVoice* v = &player->song.voices[voice];
    return (McmdVoiceState) { .note = v->last_instrument >= 0 ? (v->note + v->note_transpose) & NOTE_MASK : -1,
                              .instrument = v->last_instrument };
}
