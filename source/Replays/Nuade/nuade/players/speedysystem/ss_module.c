// SPDX-License-Identifier: MIT
// Speedy System player: the song file, see ss_internal.h.

#include "ss_internal.h"

#include "nuade_utils.h"
#include <string.h>

#define MAGIC "SPEEDY-SYSTEM"
#define MARKER 0x00001190u
// Check2 looks for the marker in 128 word steps from byte 14; InitSound looks from byte 0x12 on.
#define CHECK_START 14
#define CHECK_STEPS 128
#define SETUP_START 0x12
#define POINTER_TABLE_BYTES 0x400 // the pattern pointers of one voice
#define SAMPLE_TABLE_OFFSET 0x1000
#define SAMPLE_TABLE_ENTRIES 50
// The replayer adds 3 and 4 to every relocated sample pointer.
#define SAMPLE_POINTER_BIAS 7
// An IFF 8SVX file inside a sample: the replayer finds the chunk "BODY" at one of two fixed places, with VHDR (the
// sizes of the one shot and the repeat part) before it.
#define IFF_VHDR_ONE_SHOT 0x14
#define IFF_VHDR_REPEAT 0x18
#define IFF_BODY_PLAIN 0x28 // FORM, 8SVX and VHDR only
#define IFF_BODY_NAMED 0x60 // with the chunks that follow VHDR
#define IFF_TAG_BODY 0x424F4459u
// A chunk: tag, size, data.
#define IFF_CHUNK_SIZE 4
#define IFF_CHUNK_DATA 8
#define RAW_SAMPLE_TAIL 12 // a sample without a header loses its last 12 bytes
#define IFF_ONE_SHOT_TAIL 4
#define DELAY_NIBBLE_MASK 0x0Fu
#define END_CELL_BYTE 0xFF
#define FILL_END 0x90
#define FILL_EMPTY 0x92
#define NOTE_NIBBLE_LAST 13 // the table holds the nibbles 0..12; 13 would read past it
#define TABLE_NOTE_LAST 12
#define BASE_OCTAVE 3 // the table's own octave
#define NO_SOUND_BYTE 0xFF
#define EFFECT_FIRST 0x50
#define EFFECT_LAST 0x8F
#define NOTE_FIRST 0x10
#define SAMPLE_LAST 0x30
#define SAMPLE_SKIP_LAST 0xB0

// The replayer's period table (entry 0 is never played; the nibble 13 reads past it).

static const uint16_t k_periods[TABLE_NOTE_LAST + 1] = {
    0x1AB, 0x194, 0x17D, 0x167, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0F0, 0x0E2, 0x0D6,
};

// The attack and release delays, in ticks, indexed by the nibbles of a note cell's second byte.
static const uint8_t k_delays[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 24 };

static bool in_range(size_t size, int64_t offset, size_t length) {
    return offset >= 0 && (uint64_t)offset <= size && length <= size - (size_t)offset;
}

// 0 past the end; callers check the range where it matters.

static uint32_t read_u32(const uint8_t* data, size_t size, int64_t offset) {
    if (!in_range(size, offset, 4)) {
        return 0;
    }
    return nuade_read_be32(data + offset);
}

// The nibbles of a note cell's first byte.
typedef struct SsNote {
    unsigned octave; // 1..4
    unsigned note;   // 1..13
} SsNote;

static SsNote cell_note(SsCell cell) {
    return (SsNote) { .octave = cell.first >> 4, .note = cell.first & 0x0Fu };
}

SsDelays ss_cell_delays(SsCell cell) {
    return (SsDelays) { .attack = k_delays[cell.second >> 4], .release = k_delays[cell.second & DELAY_NIBBLE_MASK] };
}

SsCellKind ss_cell_kind(SsCell cell) {
    if (cell.first == END_CELL_BYTE) {
        return SsCell_End;
    }
    // The replayer tests the cell byte with "bpl" after a compare: only the bytes 0x10..0x8F get past the first test.
    if (cell.first < NOTE_FIRST || cell.first > EFFECT_LAST) {
        return SsCell_Skip;
    }
    if (cell.first >= EFFECT_FIRST) {
        return SsCell_Effect;
    }
    const unsigned note = cell_note(cell).note;
    return note >= 1 && note <= NOTE_NIBBLE_LAST ? SsCell_Note : SsCell_Skip;
}

SsNoteStart ss_cell_start(SsCell cell) {
    if (cell_note(cell).note > TABLE_NOTE_LAST) {
        return (SsNoteStart) { SsStart_Unmodelled, 0 };
    }
    if (cell.second == NO_SOUND_BYTE) {
        return (SsNoteStart) { SsStart_Silent, 0 };
    }
    if (cell.third > SAMPLE_SKIP_LAST) {
        return (SsNoteStart) { SsStart_Unmodelled, 0 };
    }
    if (cell.third > SAMPLE_LAST) {
        return (SsNoteStart) { SsStart_NoSample, 0 };
    }
    return (SsNoteStart) { SsStart_Sample, cell.third };
}

uint16_t ss_note_period(SsCell cell) {
    const SsNote note = cell_note(cell);
    // The nibble 13 is not modelled (ss_cell_start): clamp, so the table is never read past its end.
    const unsigned period = k_periods[note.note <= TABLE_NOTE_LAST ? note.note : TABLE_NOTE_LAST];
    if (note.octave < BASE_OCTAVE) {
        return (uint16_t)(period << (BASE_OCTAVE - note.octave));
    }
    return (uint16_t)(period >> (note.octave - BASE_OCTAVE));
}

// The offset of a voice's pattern at a position: the table entry is relative to the marker.

static int64_t pattern_offset(const SsModule* m, unsigned voice, uint8_t position) {
    const int32_t pointer
        = (int32_t)read_u32(m->data, m->size, (int64_t)m->base + position * 4 + voice * POINTER_TABLE_BYTES);
    return (int64_t)m->base + pointer;
}

bool ss_expand_position(const SsModule* module, uint8_t position, SsCell rows[SS_ROWS][SS_VOICES]) {
    for (unsigned voice = 0; voice < SS_VOICES; voice++) {
        int64_t offset = pattern_offset(module, voice, position);
        unsigned row = 0;
        while (row < SS_ROWS) {
            if (!in_range(module->size, offset, 3)) {
                return false;
            }
            const uint8_t* p = module->data + offset;
            if (p[0] == FILL_END || p[0] == FILL_EMPTY) {
                const uint8_t fill = p[0] == FILL_END ? END_CELL_BYTE : 0;
                for (unsigned count = p[1]; count != 0 && row < SS_ROWS; count--) {
                    rows[row++][voice] = (SsCell) { fill, fill, fill };
                }
                offset += 2;
            } else {
                rows[row++][voice] = (SsCell) { p[0], p[1], p[2] };
                offset += 3;
            }
        }
    }
    return true;
}

// AUDxLEN is the low word of the length (move.w); 0 plays 65536 words.

static uint16_t length_register(int64_t words) {
    return (uint16_t)words;
}

typedef struct SampleEntry {
    int32_t pointer;
    int32_t length;
} SampleEntry;

static SampleEntry read_sample_entry(const SsModule* m, unsigned sample) {
    const int64_t table = (int64_t)m->base + SAMPLE_TABLE_OFFSET;
    return (SampleEntry) { .pointer = (int32_t)read_u32(m->data, m->size, table + sample * 4),
                           .length = (int32_t)read_u32(m->data, m->size, table + (SAMPLE_TABLE_ENTRIES + sample) * 4) };
}

typedef struct IffHeader {
    bool has_body;
    bool named_body; // the BODY chunk follows the chunks after VHDR
    int32_t one_shot;
    int32_t repeat;
    int32_t body_size;
    uint32_t body; // the offset of the BODY chunk from the sample's start
} IffHeader;

// Reads 0 past the end of the file: a sample too close to it has no header.

static IffHeader read_iff_header(const SsModule* m, int64_t offset) {
    IffHeader h = { 0 };
    h.named_body = read_u32(m->data, m->size, offset + IFF_BODY_NAMED) == IFF_TAG_BODY;
    h.has_body = h.named_body || read_u32(m->data, m->size, offset + IFF_BODY_PLAIN) == IFF_TAG_BODY;
    h.body = h.named_body ? IFF_BODY_NAMED : IFF_BODY_PLAIN;
    h.one_shot = (int32_t)read_u32(m->data, m->size, offset + IFF_VHDR_ONE_SHOT);
    h.repeat = (int32_t)read_u32(m->data, m->size, offset + IFF_VHDR_REPEAT);
    h.body_size = (int32_t)read_u32(m->data, m->size, offset + h.body + IFF_CHUNK_SIZE);
    return h;
}

// An AUDxLC in the file: the player's padded copy has any AUDxLEN readable behind it.

static bool sample_in_file(const SsModule* m, int64_t offset) {
    return offset >= 0 && (uint64_t)offset <= m->size;
}

// The sample start (InitSound relocated the pointer, the note start writes AUDxLC/AUDxLEN, then the routine at 0x1104
// looks for an IFF header and replaces them).

static SsSample resolve_sample(const SsModule* m, unsigned sample) {
    const SampleEntry entry = read_sample_entry(m, sample);
    const int64_t address = (int64_t)m->base + entry.pointer + SAMPLE_POINTER_BIAS;
    const int64_t even = address & ~(int64_t)1;
    SsSample s = { 0 };
    s.start = (uint32_t)((address + 1) & ~(int64_t)1);
    s.start_len_words = length_register(((int64_t)entry.length - RAW_SAMPLE_TAIL) >> 1);

    const IffHeader h = read_iff_header(m, even);
    if (h.has_body) {
        s.start = (uint32_t)(even + h.body + IFF_CHUNK_DATA);
        if (h.named_body && h.repeat != 0 && h.one_shot != 0) {
            s.start_len_words = length_register(((int64_t)h.one_shot - IFF_ONE_SHOT_TAIL) >> 1);
            s.has_repeat = true;
            s.repeat = (uint32_t)(even + h.one_shot + h.body + IFF_CHUNK_SIZE);
            s.repeat_len_words = length_register((int64_t)h.repeat >> 1);
        } else if (h.named_body) {
            s.start_len_words = length_register((int64_t)h.body_size >> 1);
        } else {
            s.start_len_words = length_register(((int64_t)h.one_shot - IFF_ONE_SHOT_TAIL) >> 1);
        }
    }
    s.valid = sample_in_file(m, s.start) && (!s.has_repeat || sample_in_file(m, s.repeat));
    return s;
}

// Check2: the magic, then the marker in the first 128 words after byte 14.

static bool check_magic(const uint8_t* data, size_t size) {
    if (size < sizeof(MAGIC) - 1 || memcmp(data, MAGIC, sizeof(MAGIC) - 1) != 0) {
        return false;
    }
    for (unsigned step = 0; step < CHECK_STEPS; step++) {
        if (read_u32(data, size, CHECK_START + step * 2) == MARKER) {
            return true;
        }
    }
    return false;
}

// Every position the song plays up to the first end cell of voice 0 (or the 256 positions of the byte-wide position):
// each pattern must lie in the file, each sample a note starts must be a valid one, and no note may need the
// unmodelled reads.

static bool check_song(const SsModule* m) {
    for (unsigned position = 0; position < SS_POSITIONS; position++) {
        SsCell rows[SS_ROWS][SS_VOICES];
        if (!ss_expand_position(m, (uint8_t)position, rows)) {
            return false;
        }
        for (unsigned row = 0; row < SS_ROWS; row++) {
            for (unsigned voice = 0; voice < SS_VOICES; voice++) {
                const SsCell cell = rows[row][voice];
                const SsCellKind kind = ss_cell_kind(cell);
                if (kind == SsCell_End && voice == 0) {
                    return true;
                }
                if (kind != SsCell_Note) {
                    continue;
                }
                const SsNoteStart start = ss_cell_start(cell);
                if (start.kind == SsStart_Unmodelled
                    || (start.kind == SsStart_Sample && !m->samples[start.sample].valid)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool ss_module_parse(SsModule* module, const uint8_t* data, size_t size) {
    if (!check_magic(data, size)) {
        return false;
    }
    *module = (SsModule) { .data = data, .size = size };
    uint32_t base = SETUP_START;
    while (read_u32(data, size, base) != MARKER) {
        base += 2;
        if (!in_range(size, base, 4)) {
            return false;
        }
    }
    module->base = base;
    module->speed = data[base - 1];
    if (!in_range(size, (int64_t)base + SAMPLE_TABLE_OFFSET, SAMPLE_TABLE_ENTRIES * 8)) {
        return false;
    }
    for (unsigned sample = 0; sample < SS_SAMPLES; sample++) {
        module->samples[sample] = resolve_sample(module, sample);
    }
    return check_song(module);
}
