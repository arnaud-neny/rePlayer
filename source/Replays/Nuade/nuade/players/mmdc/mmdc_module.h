// SPDX-License-Identifier: MIT
// MMDC module: the OctaMED MMD0 layout with run-length compressed blocks, as the eagleplayer's InitPlayer sees it.

#pragma once

#include "nuade_base.h"
#include <nuade/players/mmdc.h>

uint8_t mmdc_u8(const MmdcImage* image, uint64_t offset);
uint16_t mmdc_u16(const MmdcImage* image, uint64_t offset);
uint32_t mmdc_u32(const MmdcImage* image, uint64_t offset);

// The eagleplayer's Check2 and InitPlayer checks, and what the replayer would read outside the module or its own
// tables: false for a module it cannot play as written (a block with other than 1..4 tracks, a block that does not
// decode inside the module, an instrument without a sample slot, a tempo that indexes past the tempo table).
bool mmdc_parse_module(const MmdcImage* image, MmdcModule* module);

void mmdc_line_reader_start(MmdcLineReader* reader, const MmdcImage* image, const MmdcBlock* block);
void mmdc_line_reader_next(MmdcLineReader* reader, uint8_t tracks, uint8_t line[MMDC_MAX_TRACKS * MMDC_CELL_SIZE]);

MmdcCell mmdc_read_cell(const uint8_t* bytes);

// The cell of a block at `line` and `track`, decoded from the start of the block.
MmdcCell mmdc_block_cell(const MmdcImage* image, const MmdcBlock* block, uint32_t line, uint32_t track);
