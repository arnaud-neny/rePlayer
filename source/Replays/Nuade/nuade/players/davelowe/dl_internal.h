// SPDX-License-Identifier: MIT
// Dave Lowe player: types shared by the module loaders and the engine.
//
// Every kind of module is one byte array in which the music data points at itself with 32-bit addresses: the hunk
// offsets of the game drivers and Deli modules (the 68k loader adds the load address to them) and the offsets from the
// module start of the ".dln" modules. The engine reads the array through typed readers and keeps its voice records in
// structs.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/davelowe.h>

// The module bytes. Reads past the end return 0 (the engine would read whatever follows in memory). The module is
// followed by NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes, then the built-in tables, and the allocation comes from
// nuade_paula_padded_copy: a sample that starts inside the module plays zeros past its end, and any address up to
// `size` has room for any AUDxLEN.

// Room for the built-in tables and the data the patches of the eagleplayers add
#define DL_BUILTIN_SIZE 0x400

static inline uint8_t dli_u8(const DlImage* image, uint32_t address) {
    return address < image->size ? image->data[address] : 0;
}

static inline uint16_t dli_u16(const DlImage* image, uint32_t address) {
    return (uint16_t)((dli_u8(image, address) << 8) | dli_u8(image, address + 1));
}

static inline uint32_t dli_u32(const DlImage* image, uint32_t address) {
    return ((uint32_t)dli_u16(image, address) << 16) | dli_u16(image, address + 2);
}

// Copies the module and appends the built-in tables; null if the allocation fails.
DlImage* dl_image_create(const uint8_t* data, size_t size);
void dl_image_destroy(DlImage* image);
// Remember the module as it is now (after the load fixes) and later put it back.
bool dl_image_keep_original(DlImage* image);
void dl_image_restore(DlImage* image);

// Writes, for the fixes the eagleplayers and the drivers apply to the module data when they load it. A write past the
// image is dropped.
static inline void dli_set16(DlImage* image, uint32_t address, uint16_t value) {
    if (image->size >= 2 && address <= image->size - 2) {
        image->data[address] = (uint8_t)(value >> 8);
        image->data[address + 1] = (uint8_t)value;
    }
}

static inline void dli_set32(DlImage* image, uint32_t address, uint32_t value) {
    if (image->size < 4 || address > image->size - 4) {
        return;
    }
    dli_set16(image, address, (uint16_t)(value >> 16));
    dli_set16(image, address + 2, (uint16_t)value);
}

// What AUDxLC set to `address` points at: the register ignores bit 0. Null past the image.
static inline const int8_t* dli_sample(const DlImage* image, uint32_t address) {
    const uint32_t word_address = address & ~1u;
    return word_address <= image->size ? (const int8_t*)(image->data + word_address) : nullptr;
}

// A read position in the image: the headers, tables and patterns are read field after field.
typedef struct DlCursor {
    const DlImage* image;
    uint32_t address;
} DlCursor;

static inline uint16_t dlc_u16(DlCursor* cursor) {
    const uint16_t value = dli_u16(cursor->image, cursor->address);
    cursor->address += 2;
    return value;
}

static inline uint32_t dlc_u32(DlCursor* cursor) {
    const uint32_t value = dli_u32(cursor->image, cursor->address);
    cursor->address += 4;
    return value;
}

// Song data

// A sample record: how a pattern selects the sound of the notes that follow. `state` 0 is the immediate form: the
// pointer and length go straight to the channel registers and the record ends after the length.
#define DL_SAMPLE_RECORD_SIZE 14

typedef struct DlSampleRecord {
    uint16_t state; // a DlSampleState, or another value that the engine treats as Looping
    uint32_t address;
    uint16_t length_words;
    uint32_t loop_address;
    uint16_t loop_length_words;
} DlSampleRecord;

DlSampleRecord dl_read_sample_record(const DlImage* image, uint32_t record);
// The load hooks name a record by the address of its sound pointer.
uint32_t dl_sample_record_of_address_field(uint32_t field);
// The fixes some load hooks apply to a record: where the sound is, and where its loop is.
void dl_write_sample_address(DlImage* image, uint32_t record, uint32_t address);
void dl_write_sample_loop(DlImage* image, uint32_t record, uint32_t loop_address, uint16_t loop_length_words);

// The last entry of a volume envelope; the entry before it is held
#define DL_ENVELOPE_END 0xff

// Command words of the standard command table (four times the index in the table) that the player writes or checks
typedef enum DlCommandWord {
    DlCommandWord_Sample = 4,
    DlCommandWord_NextPattern = 8,
    DlCommandWord_Envelope = 12,
    DlCommandWord_Filter = 0x30,
} DlCommandWord;

// Module kinds

#define DL_NOT_FOUND 0xFFFFFFFFu

// A word of a code pattern that matches any word
#define DL_ANY_WORD 0x10000u

// The game drivers and the Deli modules are hunk executables of one code hunk. A file is no module when it is smaller
// than the hunk header and this much code.
#define DL_HUNK_MIN_CODE 0x40
// Where the code hunk starts in the file; the image of a module starts there
#define DL_HUNK_CODE_OFFSET 0x20
// In the code hunk: the first instruction, then an 8 byte tag, then the header of the kind of module
#define DL_MODULE_TAG_AT 4
#define DL_MODULE_TAG_SIZE 8

// 68k opcodes that the locators of the game drivers and the load hooks look for (the first word of the instruction; the
// register letters name the operand registers of the original). The driver's voice record is addressed through a0.
enum {
    Op_Nop = 0x4e71,
    Op_Rts = 0x4e75,
    Op_JsrAbsolute = 0x4eb9,
    Op_JmpIndirectA4 = 0x4ed4,
    Op_BsrWord = 0x6100,
    Op_BeqWord = 0x6700,
    Op_BneWord = 0x6600,
    Op_BccWord = 0x6400,
    Op_DbraD0 = 0x51c8,
    Op_MoveqD0 = 0x7000,
    Op_MoveqD7 = 0x7e00,
    Op_LeaAbsoluteA0 = 0x41f9,
    Op_LeaAbsoluteA1 = 0x43f9,
    Op_LeaAbsoluteA2 = 0x45f9,
    Op_LeaAbsoluteA3 = 0x47f9,
    Op_LeaPcA0 = 0x41fa,
    Op_LeaPcA1 = 0x43fa,
    Op_LeaPcA2 = 0x45fa,
    Op_LeaDisplacedA0 = 0x41e8,
    Op_LeaDisplacedA1 = 0x43e9,
    Op_AddaLongD0ToA1 = 0xd3c0,
    Op_SubWordDisplacedA0ToD0 = 0x9068, // sub.w d16(a0),d0
    Op_SubWordAbsoluteToD0 = 0x9079,    // sub.w abs.l,d0
    Op_AddaWordPostIncA2ToA0 = 0xd0da,
    Op_MoveaLongImmediateA3 = 0x267c,
    Op_MoveaIndexedA3ToA4 = 0x2873, // movea.l (a3,d0.w),a4: takes the command's handler from the table
    Op_MoveLongA0ToIndirectA1 = 0x2288,
    Op_MoveLongA0ToDisplacedA1 = 0x2348,
    Op_MoveLongIndirectA1ToAbsolute = 0x23d1,
    Op_MoveLongImmediateToAbsolute = 0x23fc,
    Op_MoveWordImmediateD0 = 0x303c,
    Op_MoveWordImmediateD1 = 0x323c,
    Op_MoveWordImmediateToAbsolute = 0x33fc,
    Op_MoveWordD0ToAbsolute = 0x33c0,
    Op_MoveWordD0ToDisplacedA2 = 0x3540, // move.w d0,d16(a2): a write to a channel register
    Op_MoveWordD1ToDmacon = 0x33c1,      // move.w d1,$dff096.l: the DMA request that a voice returns
    Op_CmpiLongToAbsolute = 0x0cb9,
    Op_TstWordDisplacedA0 = 0x4a68,  // tst.w d16(a0)
    Op_CmpiWordDisplacedA0 = 0x0c68, // cmpi.w #imm,d16(a0)

    // The first words of the command handlers
    Op_MoveaPostIncA1ToA3 = 0x2659,             // movea.l (a1)+,a3: the sample command
    Op_MoveaDisplacedA0ToA1 = 0x2268,           // movea.l d16(a0),a1: the next pattern command
    Op_MoveaPostIncA1ToA4 = 0x2859,             // movea.l (a1)+,a4: the envelope command
    Op_MoveLongPostIncA4ToDisplacedA0 = 0x215c, // move.l (a4)+,d16(a0): the envelope command with two header words
    Op_MoveLongPostIncA1ToDisplacedA0 = 0x2159, // move.l (a1)+,d16(a0): the slide command, the early envelope command
    Op_MoveWordImmediateToDisplacedA0 = 0x317c, // move.w #imm,d16(a0): the vibrato and bonus commands
    Op_ClrWordDisplacedA0 = 0x4268,             // clr.w d16(a0): the slide off and vibrato off commands
    Op_MoveWordPostIncA1ToDisplacedA0 = 0x3159, // move.w (a1)+,d16(a0): the rest command
    Op_MoveWordPostIncA4ToDisplacedA0 = 0x315c, // move.w (a4)+,d16(a0): the envelope command with one header word
    Op_CmpiWordIndirectA0 = 0x0c50,             // cmpi.w #imm,(a0): the stop command
    Op_BsetCia = 0x08b9,                        // bset/bclr #1,$bfe001: the filter commands
    Op_BclrCia = 0x08f9,
    Op_MoveaIndirectA1ToA1 = 0x2251,    // movea.l (a1),a1: the jump command
    Op_MoveLongIndirectA1ToD3 = 0x2611, // move.l (a1),d3: the jump command, other form
    Op_TstWordIndirectA1 = 0x4a51,      // tst.w (a1): the repeat command
};

// Pattern search in the 68k code of a module: `pattern` is `count` words; DL_ANY_WORD matches any word.
bool dl_code_matches(const DlImage* image, uint32_t at, const uint32_t* pattern, uint32_t count);
uint32_t dl_code_find(const DlImage* image, uint32_t from, uint32_t to, const uint32_t* pattern, uint32_t count);
// Whether `file` is a one-hunk Amiga executable that starts with the first instruction of the modules; `code_size` is
// the size of its code hunk, which starts at DL_HUNK_CODE_OFFSET.
bool dl_hunk_code(const DlImage* file, uint32_t* code_size);

// The eagleplayer's check on the file: false if it is not a game driver. `code_size` is the size of the code hunk,
// which starts at DL_HUNK_CODE_OFFSET.
bool dl_game_extract(const DlImage* file, uint32_t* code_size);
// Runs the module's load hook on the image, finds the tables of the driver in the code hunk and the engine variant it
// has; false if it is not a known driver.
bool dl_game_locate(DlImage* image, DlGameModule* module);
// The effect of the routine the eagleplayer calls once after loading: some drivers turn offsets into addresses there.
// False if the module has a routine this player does not know.
bool dl_run_load_hook(DlImage* image, uint32_t hook);
// The Midwinter II patch of the eagleplayer: phrases of its own in some pattern lists.
void dl_patch_midwinter(DlImage* image, uint32_t size_marker, uint32_t subsong_count);
DlSubsong dl_game_subsong(const DlImage* image, const DlGameModule* module, uint32_t index);

bool dl_deli_extract(const DlImage* file, uint32_t* code_size);
bool dl_deli_locate(const DlImage* image, DlDeliModule* module);
DlSubsong dl_deli_subsong(const DlImage* image, const DlDeliModule* module, uint32_t index);
DlVariant dl_deli_variant(void);

// The eagleplayer's check (and its patch of one module): false if the data is not a ".dln" module.
bool dl_new_locate(DlImage* image, DlNewModule* module);
DlSubsong dl_new_subsong(const DlImage* image, const DlNewModule* module, uint32_t index);
DlVariant dl_new_variant(const DlNewModule* module);

// Engine

// The volume ramp of some drivers: the music volume is the envelope minus `level`, which counts down from
// DL_RAMP_START_LEVEL by one every `ticks` ticks
#define DL_RAMP_START_LEVEL 0x40

#define DL_FIRST_EFFECT_CHANNEL 2

// Prepare the voices of a subsong, as the engine's init routine does.
void dl_engine_start(DlEngine* engine, DlImage* image, const DlVariant* variant, const DlSubsong* subsong);

// One interrupt of the music voices; sets engine->loop.looped when the song loops.
void dl_engine_tick(DlEngine* engine);

// Hand the registers the tick wrote to the mixer.
void dl_engine_sync(DlEngine* engine, NuadeVoices* voices);
