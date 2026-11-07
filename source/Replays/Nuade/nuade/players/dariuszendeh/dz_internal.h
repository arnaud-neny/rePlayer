// SPDX-License-Identifier: MIT
// Darius Zendeh player: types shared by the code locator and the replayer.
//
// A module is the replayer's 68k code followed by its tables and song data. The code keeps its variables inside the
// module; the player keeps them in structs and reads everything else (lists, patterns, instruments, periods, samples)
// from the module bytes at the offsets the locator found in the code.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/players/dariuszendeh.h>

static inline uint8_t dzm_u8(const DzModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static inline uint16_t dzm_u16(const DzModule* m, uint32_t offset) {
    return (uint16_t)((dzm_u8(m, offset) << 8) | dzm_u8(m, offset + 1));
}

static inline uint32_t dzm_u32(const DzModule* m, uint32_t offset) {
    return ((uint32_t)dzm_u16(m, offset) << 16) | dzm_u16(m, offset + 2);
}

// A paula pointer to `offset` into the module; null if it lies past the end.
static inline const int8_t* dzm_sample(const DzModule* m, uint32_t offset) {
    return offset <= m->size ? (const int8_t*)(m->data + offset) : nullptr;
}

// Finds the replayer's tables and the songs the init code sets up. False when the code is not a known variant.
bool dz_locate(const DzModule* m, DzLayout* layout);
