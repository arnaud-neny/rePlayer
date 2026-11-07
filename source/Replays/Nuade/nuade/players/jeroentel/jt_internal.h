// SPDX-License-Identifier: MIT
// Jeroen Tel player: types shared by the module scan (jt_module.c) and the replayer (jeroentel.c).
//
// A module is the memory image of the game's driver. The driver's code is not run; the data it addresses is: a period
// table, a song table (0x12 bytes per song: four track list addresses, the number 12, the speed), pattern and
// instrument address tables, and arpeggio lists. Addresses in the image are absolute: the module was linked at a fixed
// load address, which the scan recovers from the period table.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/jeroentel.h>

// An offset into the module that reads zeros: the address of a record is not inside the module.
#define JT_NO_OFFSET UINT32_MAX

// The period table has three octaves (C-1 to B-3); a note beyond it uses the last entry.
#define JT_PERIOD_COUNT 36

// The module as the player keeps it: a read outside returns 0 (the original would read whatever is in memory there).
// The allocation comes from nuade_paula_padded_copy, so a sample that starts inside the module can run past its end.

uint8_t jt_u8(const JtModule* m, uint32_t offset);
uint16_t jt_u16(const JtModule* m, uint32_t offset);
uint32_t jt_u32(const JtModule* m, uint32_t offset);

// The offset of an absolute address, JT_NO_OFFSET when it is outside the module.
uint32_t jt_offset_of(const JtModule* m, uint32_t address);

// The offset the address stored at `table + byte_offset` leads to.
uint32_t jt_table_entry(const JtModule* m, uint32_t table, uint32_t byte_offset);

// The sample bytes at an absolute address (AUDxLC ignores bit 0), null when it is outside the module.
const int8_t* jt_sample_at(const JtModule* m, uint32_t address);

// The word of the period table at byte offset `index * 2`; 0 beyond the table.
uint16_t jt_period(const JtModule* m, unsigned index);

// One song of the song table; false when `number` is not a song.
typedef struct JtSong {
    uint32_t tracks[JT_VOICES]; // offsets of the voices' track lists
    uint8_t speed;              // counter reload: a step lasts speed + 2 ticks
} JtSong;

bool jt_read_song(const JtModule* m, uint32_t number, JtSong* out);

// The record at an offset; zeros when the offset is JT_NO_OFFSET.
JtInstrument jt_read_instrument(const JtModule* m, uint32_t offset);

// The record every voice points at until its first instrument command: the eagleplayer's own first bytes.
JtInstrument jt_default_instrument(void);
