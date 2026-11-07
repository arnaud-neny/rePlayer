// SPDX-License-Identifier: MIT
// SUN-Tronic: the module as the eagleplayer leaves it in memory (relocated), the build of the replayer it carries, the
// song's tables, and the checks that prove the port models the song.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/suntronic.h>

// The address UADE loads every module at. The eagleplayer relocates the module's pointer tables to it; the port keeps
// the same addresses so that a pointer the module leaves unrelocated reads the same memory.
#define SUN_MODULE_BASE 0xf000u

// A step plays 16 rows. A pattern row ends at a zero; a row longer than SUN_MAX_ROW_BYTES is not a real song.
#define SUN_ROWS_PER_STEP 16
#define SUN_MAX_ROW_BYTES 4096

// The variable block bytes InitSound and the replayer write (and A's seeds): a song's tables stay behind them.
#define SUN_VARS_VOLATILE 0x28
// Generation A: the byte that stops the replayer while it is nonzero, and the two random seed words.
#define SUN_A_HALT_FLAG 0x18
#define SUN_A_SEED 0x24
// The replayer indexes the envelope, vibrato and morph tables with a sign extended 16 bit position.
#define SUN_MAX_POSITION 0x8000

#define SUN_WAVE_BYTES_BC 0x20

uint8_t sun_mem_u8(const SunMemory* mem, uint32_t address);
uint16_t sun_mem_u16(const SunMemory* mem, uint32_t address);
uint32_t sun_mem_u32(const SunMemory* mem, uint32_t address);

// Opens the module: the eagleplayer's check and relocation, the build, and the proof that the song stays within what
// the player models. False for anything else.
bool sun_module_open(SunModule* module, const uint8_t* data, size_t size);
void sun_module_close(SunModule* module);

// Reads synth instrument `index` (below synth_count).
SunInstrument sun_module_instrument(const SunModule* module, uint32_t index);

// A step: the pattern address of each voice and, in generations A and B, the voice transpositions.
typedef struct SunStep {
    uint32_t pattern[SUN_VOICES];
    int8_t transpose[SUN_VOICES];
} SunStep;

// Reads step `index`. `end` is true for the entry that ends the table (its first pattern is 0).
SunStep sun_module_step(const SunModule* module, uint32_t index, bool* end);

// The step index InitSound writes: the first step change moves to the one after it.
uint32_t sun_module_start_step(const SunModule* module);

// The bytes of a pattern stream. A row is the items up to a zero byte.
typedef enum SunItemKind {
    SunItem_RowEnd,
    SunItem_Instrument, // value = the instrument byte, 1 .. 0x7f
    SunItem_Note,       // value = the note byte (generation A also restarts the last instrument)
    SunItem_Command,    // value = the command byte, args = its argument bytes
} SunItemKind;

typedef enum SunCommand {
    SunCommand_MasterVolume = 0x92,    // A
    SunCommand_Fade = 0x93,            // A
    SunCommand_SetPitch = 0x94,        // A
    SunCommand_RestartMorph = 0x95,    // A
    SunCommand_RestartEnvelope = 0x96, // A
    SunCommand_Seed = 0x97,            // A
    SunCommand_Speed = 0x98,
    SunCommand_Volume = 0x99,
    SunCommand_VolumeSlide = 0x9a,
    SunCommand_Slide = 0x9b,
    SunCommand_PeriodRow = 0x9c,
} SunCommand;

typedef struct SunItem {
    SunItemKind kind;
    uint8_t value;
    uint8_t args[2];
} SunItem;

// Reads the item at *address and moves it past the item. A byte in the command range that is no command of the
// generation reads as a command with no arguments (the replayer skips it).
SunItem sun_read_item(const SunMemory* mem, SunGen gen, uint32_t* address);
