// SPDX-License-Identifier: MIT
// Speedy A1 System player: the song file and the records the replayer reads from it.
//
// The file starts with a header of four longs (speed, start position, end position, 0x200), then the order list: 8
// bytes for each position, a pattern number word for each voice, 0xFFFF ends the song. At 0x1010 follow the patterns
// (16 rows of 3 bytes), then zeros, then the sample tables and, 0x280 bytes after the table's first long, the first
// sample as an IFF file ("FORM"). The sample tables are five arrays of 32 longs; their offsets count from the first
// long (`tables`): the sample's file start, its size, the start of its data, the length of its one shot part and the
// length of its repeat part. A song's first end marker (an order list word of 0xFFFF or a cell of 0xFF) ends it.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/speedya1system.h>

#define SA1_ROWS 16
#define SA1_EFFECT_FIRST 0x50 // the first cell byte that is an effect

// Cells

typedef struct Sa1Cell {
    uint8_t first;  // 0..0x4F: the note (octave * 16 + note); 0x50..0xFE: effect
    uint8_t second; // note: the attack and release delays (high and low nibble), 0: no envelope. effect: amount high
    uint8_t third;  // note: the sample number. effect: amount low
} Sa1Cell;

typedef enum Sa1Fetch {
    Sa1Fetch_Cell,
    Sa1Fetch_End,     // an order list word of 0xFFFF or a cell of 0xFF: the song ends
    Sa1Fetch_Invalid, // the position lies outside the file
} Sa1Fetch;

// The attack and release delays a note cell's second byte names (nibbles that index a table of ticks). A byte of 0
// stands for no envelope; an attack nibble of 0 for no attack.
typedef struct Sa1Delays {
    uint16_t attack;
    uint16_t release;
} Sa1Delays;

Sa1Delays sa1_cell_delays(Sa1Cell cell);

// The period of a cell's note, 0 for the note numbers the table leaves out (no note).
uint16_t sa1_note_period(uint8_t first);

// Samples

// Check2 and the replayer's setup, then a walk through the song. False for a file the eagleplayer rejects or the port
// does not model. A sample must start in the file; it may run past its end (the reference's memory behind the module
// reads as zero, the player's padded copy too).
bool sa1_module_parse(Sa1Module* module, const uint8_t* data, size_t size);

// The cell of a voice at a position and row.
Sa1Fetch sa1_fetch_cell(const Sa1Module* module, uint32_t position, unsigned row, unsigned voice, Sa1Cell* cell);
