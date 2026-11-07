// SPDX-License-Identifier: MIT
// A Sound Programming Language module as the eagleplayer sees it: an Amiga hunk executable with one code hunk. The code
// hunk holds a header, the replayer's code and variables, the sequence data and the samples; the eagleplayer
// relocates it, calls two routines named in the header and patches the code to report the end of the song.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/spl.h>

// null when the eagleplayer would not play the data or the port does not know its replayer.
SplModule* spl_module_open(const uint8_t* data, size_t size);
void spl_module_close(SplModule* module);

// The loader places the hunk at the build's `load_base` and relocates it, so a pointer in the module is an address; the
// offsets of the build tables are offsets of the hunk. Addresses outside the hunk (a table entry that is 0) read as 0
// and ignore writes.

static inline uint32_t spl_module_address(const SplModule* module, uint32_t offset) {
    return module->build->load_base + offset;
}

// Restores the hunk as loaded.
void spl_module_reset(SplModule* module);

uint8_t spl_module_u8(const SplModule* module, uint32_t at);
uint16_t spl_module_u16(const SplModule* module, uint32_t at);
uint32_t spl_module_u32(const SplModule* module, uint32_t at);
void spl_module_write32(SplModule* module, uint32_t at, uint32_t value);

// A pointer for AUDxLC at address `at` (bit 0 ignored); null when it lies outside the hunk and its padding.
const int8_t* spl_module_chip_pointer(const SplModule* module, uint32_t at);

// The builds of the port.
extern const SplBuild spl_builds[];
extern const uint32_t spl_build_count;
