// SPDX-License-Identifier: MIT
// Fred Gray player: the module file (an Amiga hunk executable), its header and the typed readers of its records.

#include "fg_internal.h"
#include <stdlib.h>
#include <string.h>

// The hunk executable

#define HUNK_HEADER 0x3f3
#define HUNK_CODE 0x3e9
#define HUNK_RELOC32 0x3ec
#define HUNK_END 0x3f2

// File layout: hunk header (no resident names, a table of one hunk with the first and last hunk 0), the table entry
// (size in longs, memory flags in the top bits), HUNK_CODE and its size; the code follows
#define FILE_NAMES 4
#define FILE_TABLE_SIZE 8
#define FILE_FIRST_HUNK 0x0c
#define FILE_LAST_HUNK 0x10
#define FILE_TABLE_ENTRY 0x14
#define FILE_CODE_HUNK 0x18
#define FILE_CODE 0x20

// Size in longs; the memory flags are the top two bits
#define HUNK_SIZE_MASK 0x3fffffffu
#define MAX_CODE_LONGS 0x40000

// The magic of the code start: `moveq #-1,d0; rts`, the name FRED GRAY
#define MAGIC_ENTRY 0x70ff4e75
#define MAGIC_FRED 0x46524544
#define MAGIC_GRAY 0x47524159

// The eagleplayer's check of the code: `moveq #-1,d0; rts`, the name FRED GRAY, and three non-zero longs
#define CODE_MAGIC_ENTRY 0
#define CODE_MAGIC_NAME 4
#define CODE_TICK 0x0c
#define CODE_INIT 0x10
#define CODE_SUBSONGS 0x14
#define CODE_TITLE 0x18
#define CODE_AUTHOR 0x1c
#define CODE_HEADER_END 0x2c

// A big-endian long of the file, 0 when it is not completely inside
static uint32_t file_u32(const uint8_t* data, size_t size, size_t at) {
    return at < size && size - at >= 4 ? nuade_read_be32(data + at) : 0;
}

static int compare_u32(const void* a, const void* b) {
    const uint32_t x = *(const uint32_t*)a;
    const uint32_t y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

// The relocation table after the code: HUNK_RELOC32, then (count, hunk, offsets) groups ended by a zero count, then
// HUNK_END. Only hunk 0 (the code itself) can be relocated.
static bool read_relocs(FgModule* m, const uint8_t* data, size_t size, size_t at) {
    uint32_t capacity = 0;
    if (file_u32(data, size, at) == HUNK_RELOC32) {
        at += 4;
        for (;;) {
            const uint32_t count = file_u32(data, size, at);
            at += 4;
            if (count == 0) {
                break;
            }
            if (file_u32(data, size, at) != 0 || at >= size || count > (size - at) / 4) {
                return false;
            }
            at += 4;
            if (m->reloc_count + count > capacity) {
                capacity = m->reloc_count + count;
                uint32_t* grown = (uint32_t*)realloc(m->relocs, capacity * sizeof(uint32_t));
                if (grown == nullptr) {
                    return false;
                }
                m->relocs = grown;
            }
            for (uint32_t i = 0; i < count; i++, at += 4) {
                const uint32_t offset = file_u32(data, size, at);
                if ((offset & 1) != 0 || !fg_in_module(m, offset, 4)) {
                    return false;
                }
                m->relocs[m->reloc_count++] = offset;
            }
        }
    }
    if (file_u32(data, size, at) != HUNK_END || at + 4 != size) {
        return false;
    }
    if (m->reloc_count > 1) {
        qsort(m->relocs, m->reloc_count, sizeof(uint32_t), compare_u32);
    }
    return true;
}

bool fg_module_load(FgModule* m, const uint8_t* data, size_t size) {
    memset(m, 0, sizeof(*m));
    // One code hunk, with a non-zero memory flag byte (the eagleplayer tests it), and the room for the check
    if (size < FILE_CODE + CODE_HEADER_END || file_u32(data, size, 0) != HUNK_HEADER
        || file_u32(data, size, FILE_NAMES) != 0 || file_u32(data, size, FILE_TABLE_SIZE) != 1
        || file_u32(data, size, FILE_FIRST_HUNK) != 0 || file_u32(data, size, FILE_LAST_HUNK) != 0
        || data[FILE_TABLE_ENTRY] == 0 || file_u32(data, size, FILE_CODE_HUNK) != HUNK_CODE) {
        return false;
    }
    const uint32_t longs = file_u32(data, size, FILE_TABLE_ENTRY) & HUNK_SIZE_MASK;
    if (longs != file_u32(data, size, FILE_CODE_HUNK + 4) || longs > MAX_CODE_LONGS
        || FILE_CODE + (size_t)longs * 4 > size) {
        return false;
    }
    m->size = longs * 4;
    m->image = nuade_paula_padded_copy(data + FILE_CODE, m->size);
    if (m->image == nullptr) {
        return false;
    }
    const uint32_t subsongs = fg_u32(m, CODE_SUBSONGS);
    if (fg_u32(m, CODE_MAGIC_ENTRY) != MAGIC_ENTRY || fg_u32(m, CODE_MAGIC_NAME) != MAGIC_FRED
        || fg_u32(m, CODE_MAGIC_NAME + 4) != MAGIC_GRAY || fg_u32(m, CODE_TICK) == 0 || fg_u32(m, CODE_INIT) == 0
        || subsongs == 0 || subsongs > FG_MAX_SUBSONGS || !read_relocs(m, data, size, FILE_CODE + (size_t)m->size)) {
        fg_module_free(m);
        return false;
    }
    return true;
}

void fg_module_free(FgModule* m) {
    free(m->image);
    free(m->relocs);
    memset(m, 0, sizeof(*m));
}

bool fg_module_is_reloc(const FgModule* m, uint32_t at) {
    return bsearch(&at, m->relocs, m->reloc_count, sizeof(uint32_t), compare_u32) != nullptr;
}

uint32_t fg_header_tick(const FgModule* m) {
    return fg_u32(m, CODE_TICK);
}

uint32_t fg_header_init(const FgModule* m) {
    return fg_u32(m, CODE_INIT);
}

uint32_t fg_header_subsongs(const FgModule* m) {
    return fg_u32(m, CODE_SUBSONGS);
}

// A header string, or "" if the offset is outside the code or the string is not ended inside it
static const char* header_string(const FgModule* m, uint32_t field) {
    const uint32_t at = fg_u32(m, field);
    if (at >= m->size || memchr(m->image + at, 0, m->size - at) == nullptr) {
        return "";
    }
    return (const char*)(m->image + at);
}

const char* fg_header_title(const FgModule* m) {
    return header_string(m, CODE_TITLE);
}

const char* fg_header_author(const FgModule* m) {
    return header_string(m, CODE_AUTHOR);
}

FgInstrument fg_read_instrument(const FgModule* m, uint32_t at) {
    return (FgInstrument) { .vibrato_row = fg_u8(m, at),
                            .vibrato_delay = fg_u8(m, at + 1),
                            .envelope_offset = fg_u8(m, at + 2),
                            .envelope_speed = fg_u8(m, at + 3) };
}

FgCommand fg_read_command(const FgModule* m, uint32_t at) {
    return (FgCommand) { .pitch = fg_u16(m, at + FgCommandField_Pitch),
                         .target = fg_u16(m, at + FgCommandField_Target),
                         .step = fg_u16(m, at + FgCommandField_Step),
                         .duration = fg_u16(m, at + FgCommandField_Duration),
                         .instrument = fg_read_instrument(m, at + FgCommandField_Instrument),
                         .next_command = fg_u8(m, at + FgCommandField_NextCommand),
                         .flags = fg_u8(m, at + FgCommandField_Flags) };
}
