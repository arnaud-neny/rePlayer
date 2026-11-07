// SPDX-License-Identifier: MIT
// Fred Gray player: types shared by the module loader, the code locator, the section decoder and the engine.
//
// A module is a one-hunk Amiga executable: the game's music code with its tables and song data. The module keeps its
// voice records and variables inside that hunk; the player keeps them in structs and reads the tables and the wave
// data from the module bytes at the offsets the locator found in the code. The song sections are 68k routines (one
// per section); the section decoder turns each into a list of FgOp.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/fredgray.h>

#define FG_NOT_FOUND 0xFFFFFFFFu

// The section request is one byte, so a module has at most this many subsongs
#define FG_MAX_SUBSONGS 256

// Sizes of the module's tables and waves
#define FG_WAVE_BYTES 16 // a voice's wave: AUDxLEN 8 words
#define FG_WAVE_WORDS (FG_WAVE_BYTES / 2)
#define FG_SAMPLE_BYTES 0x100 // the kick sample
#define FG_SAMPLE_WORDS (FG_SAMPLE_BYTES / 2)
#define FG_WAVE_TABLE_BYTES (16 * FG_WAVE_BYTES) // the waves a command's flags can pick
#define FG_PERIOD_STEPS 0x30                     // steps to the octave in the period table
#define FG_PERIOD_TABLE_BYTES (2 * FG_PERIOD_STEPS)
#define FG_INSTRUMENT_SIZE 4

// The `len` bytes at `at` are inside the module (written so that it cannot wrap)
static inline bool fg_in_module(const FgModule* m, uint32_t at, uint32_t len) {
    return at < m->size && len <= m->size - at;
}

static inline uint8_t fg_u8(const FgModule* m, uint32_t at) {
    return fg_in_module(m, at, 1) ? m->image[at] : 0;
}

static inline uint16_t fg_u16(const FgModule* m, uint32_t at) {
    return fg_in_module(m, at, 2) ? nuade_read_be16(m->image + at) : 0;
}

static inline uint32_t fg_u32(const FgModule* m, uint32_t at) {
    return fg_in_module(m, at, 4) ? nuade_read_be32(m->image + at) : 0;
}

static inline void fg_set16(FgModule* m, uint32_t at, uint16_t value) {
    if (fg_in_module(m, at, 2)) {
        m->image[at] = (uint8_t)(value >> 8);
        m->image[at + 1] = (uint8_t)value;
    }
}

// Reads the hunk executable and copies its code; false if it is not a one-hunk module with the FREDGRAY header
bool fg_module_load(FgModule* m, const uint8_t* data, size_t size);
void fg_module_free(FgModule* m);
// The long at `at` is an address the loader relocates
bool fg_module_is_reloc(const FgModule* m, uint32_t at);

// Header fields (code offsets): the interrupt routine, the init routine, the subsong count and the text strings
uint32_t fg_header_tick(const FgModule* m);
uint32_t fg_header_init(const FgModule* m);
uint32_t fg_header_subsongs(const FgModule* m);
const char* fg_header_title(const FgModule* m);
const char* fg_header_author(const FgModule* m);

// Module records. A command record is the block of a voice record from the pitch to the flags, so the command table
// and the voice records share it.

// Offsets in a command record
typedef enum FgCommandField {
    FgCommandField_Pitch = 0x00,
    FgCommandField_Target = 0x02,
    FgCommandField_Step = 0x04,
    FgCommandField_Duration = 0x06,
    FgCommandField_Instrument = 0x08,
    FgCommandField_NextCommand = 0x0c,
    FgCommandField_Flags = 0x0d,
    FgCommandField_Size = 0x10,
} FgCommandField;

// Offsets in a voice record. The locator also reads the three marked fields out of the code (d16(a6)).
typedef enum FgRecordField {
    FgRecordField_Period = 0x00,
    FgRecordField_EnvelopeStep = 0x04,
    FgRecordField_Volume = 0x05,
    FgRecordField_ChannelShift = 0x07,
    FgRecordField_EnvelopeTick = 0x08,
    FgRecordField_Active = 0x0a,
    FgRecordField_SlideStep = FgRecordField_Active + FgCommandField_Step, // read from the code
    FgRecordField_KickBits = 0x1a,
    FgRecordField_Command = 0x1b, // read from the code
    FgRecordField_Vibrato = 0x1c,
    FgRecordField_VibratoLeft = 0x1e,
    FgRecordField_VibratoPhase = 0x1f,
    FgRecordField_LcRegister = 0x20, // Eco, read from the code: the address of the voice's AUDxLC register
} FgRecordField;

FgInstrument fg_read_instrument(const FgModule* m, uint32_t at);
FgCommand fg_read_command(const FgModule* m, uint32_t at);

// The four engine versions found so far. The engine is told apart by a signature of its code (fg_locate.c); the rest of
// what differs between the versions is listed here.

// The masks a note gets: 7 bits (bclr #7, andi.w #$7f), or only bit 7 of the low byte cleared (Black Lamp)
#define FG_NOTE_MASK_7BITS 0x007f
#define FG_NOTE_MASK_CLEAR_BIT7 0xff7f

typedef struct FgTraits {
    const char* name;
    FgFeatures features;
} FgTraits;

const FgTraits* fg_traits(FgVersion version);

// A song section as a list of operations (see fg_section.h). The "step" is the section's working copy of the
// position; a Branch tests it and jumps to `jump` (an index into the section's list).

#define FG_NO_JUMP 0xffff

// Where the engine finds its tables, from the operands of the code

// Finds the engine in the code and decodes the sections; false (with the layout cleared) for unknown code.
bool fg_locate(const FgModule* m, FgLayout* layout);
void fg_layout_free(FgLayout* layout);
