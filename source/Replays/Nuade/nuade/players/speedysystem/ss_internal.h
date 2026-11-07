// SPDX-License-Identifier: MIT
// Speedy System player: the song file and the records the replayer reads from it.
//
// The file is "SPEEDY-SYSTEM", a name string, a 24 bit size and the speed byte, then the marker long 0x00001190. The
// pattern pointers and the sample table count their offsets from the marker's address (`base`). From `base`: the
// pattern pointer table (4 voices x 256 positions, one long each, a voice's table 0x400 bytes on from the last), from
// 0x1000 on the sample pointers (50 longs; they point 7 bytes before the data) and then the sample lengths (50
// longs). A pattern is 16 rows of three bytes for one voice; the byte 0x90 n stands for n end cells, 0x92 n for n
// empty cells.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/speedysystem.h>

#define SS_ROWS 16
#define SS_POSITIONS 256

// Cells

typedef struct SsCell {
    uint8_t first;  // 0xFF: end of the song (voice 0); 0x10..0x4F: octave and note; 0x50..0x8F: effect
    uint8_t second; // note: the attack and release delays (high and low nibble), 0xFF: no sound. effect: amount high
    uint8_t third;  // note: the sample number. effect: amount low
} SsCell;

typedef enum SsCellKind {
    SsCell_Skip,   // the replayer does nothing with the cell
    SsCell_End,    // 0xFF: voice 0 restarts the song, the other voices skip it
    SsCell_Effect, // 0x50..0x8F
    SsCell_Note,   // 0x10..0x4F with a note nibble of 1..13
} SsCellKind;

SsCellKind ss_cell_kind(SsCell cell);

// The attack and release delays a note cell's second byte names (nibbles that index a table of ticks).
typedef struct SsDelays {
    uint16_t attack;
    uint16_t release;
} SsDelays;

SsDelays ss_cell_delays(SsCell cell);

// What a note cell starts.
typedef enum SsStart {
    SsStart_Silent,     // second byte 0xFF: the voice is stopped
    SsStart_NoSample,   // the third byte is 0x31..0xB0: the delays are set, the voice stays stopped
    SsStart_Sample,     // the sample `sample`
    SsStart_Unmodelled, // reads the replayer's own variables past a table: not modelled
} SsStart;

typedef struct SsNoteStart {
    SsStart kind;
    unsigned sample;
} SsNoteStart;

SsNoteStart ss_cell_start(SsCell cell);

// The period of a note cell: the table entry of the note nibble, moved by the octave.
uint16_t ss_note_period(SsCell cell);

// Samples

// Check2 and the replayer's setup, then a walk through the song. False for a file the eagleplayer rejects or the port
// does not model. A sample must start in the file; it may run past its end (the reference's memory behind the module
// reads as zero, the player's padded copy too).
bool ss_module_parse(SsModule* module, const uint8_t* data, size_t size);

// The 16 rows of the 4 voices at a song position (the replayer's pattern expansion). False when a pattern runs past
// the file.
bool ss_expand_position(const SsModule* module, uint8_t position, SsCell rows[SS_ROWS][SS_VOICES]);
