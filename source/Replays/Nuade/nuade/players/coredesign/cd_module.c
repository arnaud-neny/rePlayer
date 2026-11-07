// SPDX-License-Identifier: MIT
// Core Design player: the module check, the hunk loader and the table locating. The eagleplayer scans the driver's code
// for instruction patterns and reads the offsets out of them; so does this. "xxxx" in a pattern is any word.

#include "cd_internal.h"
#include <stdlib.h>
#include <string.h>

#define HUNK_HEADER 0x3F3
#define HUNK_CODE 0x3E9
#define HUNK_DATA 0x3EA
#define HUNK_RELOC32 0x3EC
#define HUNK_DEBUG 0x3F1
#define HUNK_END 0x3F2
#define HUNK_TYPE_MASK 0x3FFFFFFFu // the top bits of a hunk's type and size are memory flags
#define HUNK_FLAGS_SHIFT 24
#define HUNK_HEADER_LONGS 8

// The driver starts with `moveq #-1,d0; rts`, "S.PHIPPS" and the entry table of CdHeader.
#define MAGIC_MOVEQ_RTS 0x70FF4E75
#define MAGIC_S_PH 0x532E5048
#define MAGIC_IPPS 0x49505053
#define MAGIC_LONGS 3
#define HEADER_ENTRIES 10

#define MAX_HUNK_WORDS 0x400000u
#define MAX_PATTERN_WORDS 16
#define NOT_FOUND 0xFFFFFFFFu
#define SEARCH_ROUTINE 0x180 // the routines the patterns are searched in are shorter than this
#define SEARCH_CODE 0x1000   // the driver's code is shorter than this

uint8_t cd_u8(const CdMemory* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

uint16_t cd_u16(const CdMemory* m, uint32_t offset) {
    return (uint16_t)(cd_u8(m, offset) << 8 | cd_u8(m, offset + 1));
}

int16_t cd_s16(const CdMemory* m, uint32_t offset) {
    return (int16_t)cd_u16(m, offset);
}

uint32_t cd_u32(const CdMemory* m, uint32_t offset) {
    return (uint32_t)cd_u16(m, offset) << 16 | cd_u16(m, offset + 2);
}

CdSubsong cd_read_subsong(const CdMemory* m, const CdTables* tables, uint32_t index) {
    const uint32_t at = tables->subsongs + index * CD_SUBSONG_SIZE;
    return (CdSubsong) { .sound_effect = cd_u16(m, at) == 1,
                         .index = cd_u16(m, at + 2),
                         .period = cd_u16(m, at + 4),
                         .param = cd_u16(m, at + 6) };
}

CdInstrument cd_read_instrument(const CdMemory* m, uint32_t offset) {
    const uint8_t flags = cd_u8(m, offset + 5);
    return (CdInstrument) { .attack_rate = (int8_t)cd_u8(m, offset),
                            .decay_rate = (int8_t)cd_u8(m, offset + 1),
                            .sustain_level = (int8_t)cd_u8(m, offset + 2),
                            .release_rate = (int8_t)cd_u8(m, offset + 3),
                            .peak = (int8_t)cd_u8(m, offset + 4),
                            .one_shot = (flags & 1) != 0,
                            .has_attack = (flags & 2) != 0,
                            .sample = cd_u32(m, offset + 6),
                            .loop_offset = cd_s16(m, offset + 0xA),
                            .loop_words = cd_u16(m, offset + 0xC) };
}

CdVibrato cd_read_vibrato(const CdMemory* m, const CdTables* tables, uint8_t number) {
    const uint32_t at = tables->vibratos + CD_VIBRATO_SIZE * (uint32_t)number;
    const uint8_t control = cd_u8(m, at);
    return (CdVibrato) { .restart = (control & 0x80) != 0,
                         .delay = (control >> 3) & 7,
                         .steps = (uint8_t)((control & 7) + 1),
                         .sequence = at + 1 };
}

uint32_t cd_read_song_track(const CdMemory* m, const CdTables* tables, uint8_t song, int track) {
    const uint32_t at = tables->songs + (uint32_t)song * CD_SONG_SIZE + 2 * (uint32_t)track;
    return tables->songs + (uint32_t)(int32_t)cd_s16(m, at);
}

uint32_t cd_read_pattern(const CdMemory* m, const CdTables* tables, uint8_t pattern) {
    return tables->pattern_base + (uint32_t)(int32_t)cd_s16(m, tables->pattern_offsets + 2 * (uint32_t)pattern);
}

uint16_t cd_read_period(const CdMemory* m, const CdTables* tables, uint8_t note) {
    return cd_u16(m, tables->periods + (uint32_t)(2 * (int32_t)(int8_t)note));
}

// The start of the executable: the hunk header of AmigaDOS, then the first hunk's type, size and data.

typedef struct CdHunkHeader {
    uint32_t magic;
    uint32_t library_names; // 0: no resident libraries
    uint32_t hunk_count;
    uint32_t first;
    uint32_t last;
    uint32_t hunk_size; // in longs, the top byte holds memory flags
    uint32_t type;
    uint32_t data_longs; // in the file
} CdHunkHeader;

static CdHunkHeader read_hunk_header(const uint8_t* data) {
    return (CdHunkHeader) { .magic = nuade_read_be32(data),
                            .library_names = nuade_read_be32(data + 4),
                            .hunk_count = nuade_read_be32(data + 8),
                            .first = nuade_read_be32(data + 12),
                            .last = nuade_read_be32(data + 16),
                            .hunk_size = nuade_read_be32(data + 20),
                            .type = nuade_read_be32(data + 24),
                            .data_longs = nuade_read_be32(data + 28) };
}

// The hunk of the executable, as AmigaDOS loads it. Relocation is not needed: the offsets in the hunk are the driver's
// addresses, so the hunk is kept at base 0. The eagleplayer's check (Check2) also wants the memory flags of the hunk
// to be set.

static bool load_hunk(CdMemory* m, const uint8_t* data, size_t size) {
    if (size < 4 * HUNK_HEADER_LONGS) {
        return false;
    }
    const CdHunkHeader header = read_hunk_header(data);
    const uint32_t hunk_words = header.hunk_size & HUNK_TYPE_MASK;
    const uint32_t type = header.type & HUNK_TYPE_MASK;
    const size_t data_offset = 4 * HUNK_HEADER_LONGS;
    if (header.magic != HUNK_HEADER || header.library_names != 0 || header.hunk_count != 1 || header.first != 0
        || header.last != 0 || (header.hunk_size >> HUNK_FLAGS_SHIFT) == 0 || (type != HUNK_CODE && type != HUNK_DATA)
        || hunk_words > MAX_HUNK_WORDS || header.data_longs > hunk_words
        || data_offset + 4 * (size_t)header.data_longs + 4 > size) {
        return false;
    }
    const uint32_t end_type = nuade_read_be32(data + data_offset + 4 * (size_t)header.data_longs);
    if (end_type != HUNK_RELOC32 && end_type != HUNK_DEBUG && end_type != HUNK_END) {
        return false;
    }
    m->size = hunk_words * 4;
    m->data = nuade_paula_padded_copy(nullptr, m->size);
    if (m->data == nullptr) {
        return false;
    }
    memcpy(m->data, data + data_offset, 4 * (size_t)header.data_longs);
    return true;
}

// Pattern scan

static int parse_pattern(const char* pattern, uint16_t* words, bool* any) {
    int count = 0;
    while (*pattern != '\0' && count < MAX_PATTERN_WORDS) {
        if (*pattern == ' ') {
            pattern++;
            continue;
        }
        char digits[5] = { pattern[0], pattern[1], pattern[2], pattern[3], '\0' };
        any[count] = strcmp(digits, "xxxx") == 0;
        words[count] = any[count] ? 0 : (uint16_t)strtoul(digits, nullptr, 16);
        pattern += 4;
        count++;
    }
    return count;
}

// The offset of the first match of `pattern` in [from, to), or NOT_FOUND.

static uint32_t find_pattern(const CdMemory* m, uint32_t from, uint32_t to, const char* pattern) {
    uint16_t words[MAX_PATTERN_WORDS];
    bool any[MAX_PATTERN_WORDS];
    const uint32_t count = (uint32_t)parse_pattern(pattern, words, any);
    for (uint32_t at = from; at + 2 * count <= m->size && at < to; at += 2) {
        uint32_t i = 0;
        while (i < count && (any[i] || cd_u16(m, at + 2 * i) == words[i])) {
            i++;
        }
        if (i == count) {
            return at;
        }
    }
    return NOT_FOUND;
}

// The address that the d16(pc) operand in word `operand` of the match refers to.

static uint32_t pc_operand(const CdMemory* m, uint32_t match, uint32_t operand) {
    const uint32_t extension = match + 2 * operand;
    return extension + (uint32_t)(int32_t)cd_s16(m, extension);
}

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static bool has_magic(const CdMemory* m) {
    return cd_u32(m, 0) == MAGIC_MOVEQ_RTS && cd_u32(m, 4) == MAGIC_S_PH && cd_u32(m, 8) == MAGIC_IPPS;
}

static CdHeader read_header(const CdMemory* m) {
    uint32_t entry[HEADER_ENTRIES];
    for (uint32_t i = 0; i < HEADER_ENTRIES; i++) {
        entry[i] = cd_u32(m, 4 * (MAGIC_LONGS + i));
    }
    return (CdHeader) { .tick = entry[0],
                        .audio_irq = entry[1],
                        .start = entry[2],
                        .stop = entry[3],
                        .subsongs = entry[4],
                        .instruments = entry[5],
                        .instruments_end = entry[6],
                        .title = entry[7],
                        .author = entry[8],
                        .notice = entry[9] };
}

// The tables and the version, from the operands of the routines that use them. False if a pattern is missing.

static bool locate(CdModule* module) {
    const CdMemory* m = &module->memory;
    const CdHeader* h = &module->header;
    CdTables* t = &module->tables;
    const uint32_t code_end = min_u32(m->size, h->stop + SEARCH_CODE);

    // The start routine: the subsong table, and the call that starts a song.
    const uint32_t start_end = h->start + SEARCH_ROUTINE;
    const uint32_t subsongs = find_pattern(m, h->start, start_end, "41f9 xxxx xxxx d1c2");
    const uint32_t song_call = find_pattern(m, h->start, start_end, "3028 0002 6100 xxxx");
    if (subsongs == NOT_FOUND || song_call == NOT_FOUND) {
        return false;
    }
    t->subsongs = cd_u32(m, subsongs + 2);
    const uint32_t start_song = pc_operand(m, song_call, 3);

    // The start of a song: the song table.
    const uint32_t songs = find_pattern(m, start_song, start_song + SEARCH_ROUTINE, "41fa xxxx 49f0 0000");
    if (songs == NOT_FOUND) {
        return false;
    }
    t->songs = pc_operand(m, songs, 1);

    // A track reads its order list and patterns; a note and the vibrato read the period table.
    const uint32_t patterns = find_pattern(m, h->stop, code_end, "41fa xxxx 4880 d040 3030 0000 41fa xxxx d0c0");
    const uint32_t vibratos = find_pattern(m, h->stop, code_end, "41fa xxxx 102c 0020 e748 0240 00ff d0c0");
    const uint32_t periods = find_pattern(m, h->stop, code_end, "43fa xxxx 3e31 0000");
    const uint32_t note = find_pattern(m, h->stop, code_end, "d02c 0013 0600 xxxx 1940 0017");
    if (patterns == NOT_FOUND || vibratos == NOT_FOUND || periods == NOT_FOUND || note == NOT_FOUND) {
        return false;
    }
    t->pattern_offsets = pc_operand(m, patterns, 1);
    t->pattern_base = pc_operand(m, patterns, 7);
    t->vibratos = pc_operand(m, vibratos, 1);
    t->periods = pc_operand(m, periods, 1);
    module->version.note_offset = cd_u8(m, note + 7);

    // The audio interrupt handler: the silent buffer, and the flag test that tells the generations apart: the early
    // driver tests its armed flag (bit 1) with `btst #1,$1c(a0); beq`, the late drivers theirs (bit 6) with
    // `btst #6,$1c(a0)`.
    const uint32_t irq_end = h->audio_irq + SEARCH_ROUTINE;
    const uint32_t silence = find_pattern(m, h->audio_irq, irq_end, "217c xxxx xxxx 000e");
    const bool early = find_pattern(m, h->audio_irq, irq_end, "0828 0001 001c 6700") != NOT_FOUND;
    const bool late = find_pattern(m, h->audio_irq, irq_end, "0828 0006 001c") != NOT_FOUND;
    if (silence == NOT_FOUND || early == late) {
        return false;
    }
    t->silence = cd_u32(m, silence + 2);
    module->version.attack_flag = late;
    module->version.effect_voice_by_param = late;
    return true;
}

// The eagleplayer wants the entry points and the subsong count non-zero.

static bool header_checked(const CdHeader* h) {
    return h->tick != 0 && h->audio_irq != 0 && h->start != 0 && h->stop != 0 && h->subsongs != 0;
}

// Every table must lie inside the hunk, and the entry points in the code.

static bool tables_inside(const CdModule* module) {
    const CdHeader* h = &module->header;
    const CdTables* t = &module->tables;
    const uint32_t size = module->memory.size;
    const uint32_t entry_points[] = { h->tick, h->audio_irq, h->start, h->stop };
    for (size_t i = 0; i < nuade_sizeof_array(entry_points); i++) {
        if (entry_points[i] >= size) {
            return false;
        }
    }
    const uint32_t tables[] = { t->subsongs, t->songs,   t->pattern_offsets, t->pattern_base,
                                t->vibratos, t->periods, t->silence,         h->instruments };
    for (size_t i = 0; i < nuade_sizeof_array(tables); i++) {
        if (tables[i] >= size) {
            return false;
        }
    }
    return h->instruments <= h->instruments_end;
}

bool cd_module_load(CdModule* module, const uint8_t* data, size_t size) {
    memset(module, 0, sizeof(*module));
    if (!load_hunk(&module->memory, data, size)) {
        return false;
    }
    if (has_magic(&module->memory)) {
        module->header = read_header(&module->memory);
        if (header_checked(&module->header) && locate(module) && tables_inside(module)) {
            return true;
        }
    }
    cd_module_free(module);
    return false;
}

// The eagleplayer's check (Check2), and that the tables can be found

bool cd_probe(const uint8_t* data, size_t size) {
    CdModule module;
    const bool loaded = cd_module_load(&module, data, size);
    if (loaded) {
        cd_module_free(&module);
    }
    return loaded;
}

void cd_module_free(CdModule* module) {
    free(module->memory.data);
    module->memory.data = nullptr;
}
