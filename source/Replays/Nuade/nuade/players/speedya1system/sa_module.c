// SPDX-License-Identifier: MIT
// Speedy A1 System player: the song file, see sa_internal.h.

#include "sa_internal.h"

#include "nuade_utils.h"

// The header: four longs
#define HEADER_SPEED 0
#define HEADER_START_POSITION 4
#define HEADER_END_POSITION 8
#define HEADER_TAG_FIELD 0xC
#define HEADER_TAG 0x200u
#define ORDER_LIST_OFFSET 0x10
#define ORDER_LIST_END 0x1010 // where the patterns start
#define ORDER_BYTES_PER_POSITION 8
#define ORDER_END_WORD 0xFFFF
#define PATTERN_BYTES 0x30
#define CELL_BYTES 3
#define END_CELL_BYTE 0xFF
#define FORM_TAG 0x464F524Du
#define TABLES_TO_FORM 0x280 // the five sample tables lie before the first sample
#define TABLE_BYTES 0x80
#define TABLE_SIZE 1 // the tables: file start (0), size, data start, one shot length, repeat length
#define TABLE_DATA 2
#define TABLE_ONE_SHOT 3
#define TABLE_REPEAT 4
#define IFF_HEADER_BYTES 0x68 // a sample's size includes its IFF header
#define MAX_SAMPLE_NUMBER 0x1F
// The walk ends at the first fetch outside the file; this bounds it for a position counter that never leaves it.
#define MAX_WALKED_POSITIONS 0x8000
#define DELAY_NIBBLE_MASK 0x0Fu

// The replayer's period table: 16 entries for each of the 5 octaves, 0 for the numbers that are not notes.

static const uint16_t k_periods[SA1_EFFECT_FIRST] = {
    0,    3416, 3232, 3048, 2872, 2712, 2360, 2416, 2280, 2152, 2032, 1920, 1808, 1712, 0,   0,   0,   1708, 1616, 1524,
    1436, 1356, 1280, 1208, 1140, 1076, 1016, 960,  904,  856,  0,    0,    0,    854,  808, 762, 718, 678,  640,  604,
    570,  538,  508,  480,  452,  428,  0,    0,    0,    427,  404,  381,  359,  339,  320, 302, 285, 269,  254,  240,
    226,  214,  0,    0,    0,    214,  202,  191,  180,  170,  160,  151,  143,  135,  127, 120, 113, 107,  0,    0,
};

// The attack and release delays, in ticks, indexed by the nibbles of a note cell's second byte.
static const uint8_t k_delays[16] = { 0, 1, 2, 3, 4, 6, 9, 13, 18, 24, 31, 39, 48, 50, 61, 72 };

Sa1Delays sa1_cell_delays(Sa1Cell cell) {
    return (Sa1Delays) { .attack = k_delays[cell.second >> 4], .release = k_delays[cell.second & DELAY_NIBBLE_MASK] };
}

uint16_t sa1_note_period(uint8_t first) {
    return first < nuade_sizeof_array(k_periods) ? k_periods[first] : 0;
}

static bool in_range(size_t size, int64_t offset, size_t length) {
    return offset >= 0 && (uint64_t)offset <= size && length <= size - (size_t)offset;
}

// 0 past the end of the file; callers check the range where it matters.

static uint32_t read_u32(const uint8_t* data, size_t size, int64_t offset) {
    if (!in_range(size, offset, 4)) {
        return 0;
    }
    return nuade_read_be32(data + offset);
}

static uint16_t read_u16(const uint8_t* data, size_t size, int64_t offset) {
    if (!in_range(size, offset, 2)) {
        return 0;
    }
    return nuade_read_be16(data + offset);
}

// The position and the pattern number are used as signed words (muls.w).

Sa1Fetch sa1_fetch_cell(const Sa1Module* m, uint32_t position, unsigned row, unsigned voice, Sa1Cell* cell) {
    const int64_t order = ORDER_LIST_OFFSET + (int64_t)(int16_t)position * ORDER_BYTES_PER_POSITION + voice * 2;
    if (!in_range(m->size, order, 2)) {
        return Sa1Fetch_Invalid;
    }
    const uint16_t pattern = read_u16(m->data, m->size, order);
    if (pattern == ORDER_END_WORD) {
        return Sa1Fetch_End;
    }
    const int64_t offset = ORDER_LIST_END + (int64_t)(int16_t)pattern * PATTERN_BYTES + (int64_t)row * CELL_BYTES;
    if (!in_range(m->size, offset, CELL_BYTES)) {
        return Sa1Fetch_Invalid;
    }
    const uint8_t* p = m->data + offset;
    if (p[0] == END_CELL_BYTE) {
        return Sa1Fetch_End;
    }
    *cell = (Sa1Cell) { p[0], p[1], p[2] };
    return Sa1Fetch_Cell;
}

// AUDxLEN is the low word of the length (move.w); 0 plays 65536 words.

static uint16_t length_register(int32_t words) {
    return (uint16_t)words;
}

// An AUDxLC in the file: the player's padded copy has any AUDxLEN readable behind it.

static bool sample_in_file(const Sa1Module* m, int64_t offset) {
    return offset >= 0 && (uint64_t)offset <= m->size;
}

// One entry of a sample table: `table` is the file offset of the first long.

static uint32_t table_entry(const Sa1Module* m, uint32_t tables, unsigned table, unsigned sample) {
    return read_u32(m->data, m->size, (int64_t)tables + table * TABLE_BYTES + sample * 4);
}

// What the note start and the loop update read: the data start and, for a note with an envelope, the one shot and
// repeat lengths; for a note without one the size minus the IFF header.

static Sa1Sample resolve_sample(const Sa1Module* m, uint32_t tables, unsigned sample) {
    const int64_t start = (int64_t)tables + (int32_t)table_entry(m, tables, TABLE_DATA, sample);
    const uint32_t one_shot = table_entry(m, tables, TABLE_ONE_SHOT, sample);
    const uint32_t repeat = table_entry(m, tables, TABLE_REPEAT, sample);
    const int32_t size = (int32_t)table_entry(m, tables, TABLE_SIZE, sample);

    Sa1Sample s = { 0 };
    s.start = (uint32_t)start;
    s.start_len_words = length_register((int32_t)(one_shot + repeat) >> 1);
    s.envelope_valid = sample_in_file(m, start);
    if (repeat != 0) {
        s.has_repeat = true;
        s.repeat = (uint32_t)(start + one_shot);
        s.repeat_len_words = length_register((int32_t)repeat >> 1);
        s.envelope_valid = s.envelope_valid && sample_in_file(m, (int64_t)s.repeat);
    }
    s.plain_len_words = length_register(((int64_t)size - IFF_HEADER_BYTES) >> 1);
    s.plain_valid = sample_in_file(m, start);
    return s;
}

typedef struct Header {
    uint32_t speed;
    uint32_t start_position;
    uint32_t end_position;
    uint32_t tag; // 0x200
} Header;

static Header read_header(const uint8_t* data, size_t size) {
    return (Header) { .speed = read_u32(data, size, HEADER_SPEED),
                      .start_position = read_u32(data, size, HEADER_START_POSITION),
                      .end_position = read_u32(data, size, HEADER_END_POSITION),
                      .tag = read_u32(data, size, HEADER_TAG_FIELD) };
}

// Every cell the song plays up to its end: each note must start a valid sample.

static bool check_song(const Sa1Module* m) {
    uint32_t position = m->start_position;
    for (unsigned walked = 0; walked < MAX_WALKED_POSITIONS; walked++) {
        for (unsigned row = 0; row < SA1_ROWS; row++) {
            for (unsigned voice = 0; voice < SA1_VOICES; voice++) {
                Sa1Cell cell;
                const Sa1Fetch fetch = sa1_fetch_cell(m, position, row, voice, &cell);
                if (fetch != Sa1Fetch_Cell) {
                    return fetch == Sa1Fetch_End;
                }
                if (cell.first >= SA1_EFFECT_FIRST || sa1_note_period(cell.first) == 0
                    || cell.third > MAX_SAMPLE_NUMBER) {
                    continue;
                }
                const Sa1Sample* sample = &m->samples[cell.third];
                if (!(cell.second != 0 ? sample->envelope_valid : sample->plain_valid)) {
                    return false;
                }
            }
        }
        if (++position == m->end_position) {
            return true;
        }
    }
    return false;
}

// Check2. Returns the file offset of the first sample table, or -1: the order list is scanned up to its 0xFFFF for the
// highest pattern number, the zero longs behind the patterns are skipped and 0x280 bytes behind the next long the
// first sample ("FORM") must start.

static int64_t find_tables(const uint8_t* data, size_t size, const Header* header) {
    // Check2 tests the high word of the speed long and the tag.
    if (size < ORDER_LIST_END || (header->speed >> 16) != 0 || header->tag != HEADER_TAG) {
        return -1;
    }
    unsigned highest = 0;
    for (unsigned offset = ORDER_LIST_OFFSET; offset < ORDER_LIST_END; offset += 2) {
        const unsigned word = read_u16(data, size, offset);
        if (word == ORDER_END_WORD) {
            break;
        }
        if (word > 0xFF) {
            return -1;
        }
        if (word > highest) {
            highest = word;
        }
    }
    int64_t tables = ORDER_LIST_END + (int64_t)(highest + 1) * PATTERN_BYTES;
    while (in_range(size, tables, 4) && read_u32(data, size, tables) == 0) {
        tables += 4;
    }
    if (!in_range(size, tables, 4) || read_u32(data, size, tables + TABLES_TO_FORM) != FORM_TAG) {
        return -1;
    }
    return tables;
}

bool sa1_module_parse(Sa1Module* module, const uint8_t* data, size_t size) {
    const Header header = read_header(data, size);
    const int64_t tables = find_tables(data, size, &header);
    if (tables < 0) {
        return false;
    }
    *module = (Sa1Module) { .data = data,
                            .size = size,
                            .speed = header.speed,
                            .start_position = header.start_position,
                            .end_position = header.end_position };
    for (unsigned sample = 0; sample < SA1_SAMPLES; sample++) {
        module->samples[sample] = resolve_sample(module, (uint32_t)tables, sample);
    }
    return check_song(module);
}
