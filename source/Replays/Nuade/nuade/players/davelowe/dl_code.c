// SPDX-License-Identifier: MIT
// Dave Lowe player: what the loaders share. Searching 68k code in a module, and the hunk executable that holds the game
// drivers and the Deli modules.
//
// The game drivers carry their engine as 68k code, and the player finds its tables and the variant by the instructions
// around them. A pattern is a list of the words of the instructions; an operand that differs between modules is
// DL_ANY_WORD.

#include "dl_internal.h"

#define HUNK_HEADER 0x3f3
#define HUNK_CODE_SIZE_AT 0x1c
// A module starts with the first instruction of every Deli player module: moveq #-1,d0 / rts
#define MODULE_FIRST_INSTRUCTIONS 0x70ff4e75u
#define HUNK_SIZE_MASK 0x3fffffffu

bool dl_code_matches(const DlImage* image, uint32_t at, const uint32_t* pattern, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (pattern[i] != DL_ANY_WORD && dli_u16(image, at + i * 2) != pattern[i]) {
            return false;
        }
    }
    return true;
}

uint32_t dl_code_find(const DlImage* image, uint32_t from, uint32_t to, const uint32_t* pattern, uint32_t count) {
    for (uint32_t at = from; at + count * 2 <= to && at + count * 2 <= image->module_size; at += 2) {
        if (dl_code_matches(image, at, pattern, count)) {
            return at;
        }
    }
    return DL_NOT_FOUND;
}

bool dl_hunk_code(const DlImage* file, uint32_t* code_size) {
    DlCursor cursor = { .image = file, .address = 0 };
    if (file->module_size < DL_HUNK_CODE_OFFSET + DL_HUNK_MIN_CODE || dlc_u32(&cursor) != HUNK_HEADER
        || dli_u32(file, DL_HUNK_CODE_OFFSET) != MODULE_FIRST_INSTRUCTIONS) {
        return false;
    }
    const uint32_t longs = dli_u32(file, HUNK_CODE_SIZE_AT) & HUNK_SIZE_MASK;
    if ((uint64_t)longs * 4 > file->module_size - DL_HUNK_CODE_OFFSET) {
        return false;
    }
    *code_size = longs * 4;
    return true;
}
