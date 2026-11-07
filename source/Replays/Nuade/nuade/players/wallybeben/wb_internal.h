// SPDX-License-Identifier: MIT
// Wally Beben: the module image, the known replayer builds and the readers of the module's tables.
//
// A module is the composer's replayer followed by its data, assembled at an address of its own (`origin`) and loaded
// anywhere: the eagleplayer's InitPlayer rewrites every absolute address of the code and the pointers of the song,
// pattern and sample tables to the load address. This port keeps module offsets instead (address - origin).
// The replayer's variables sit between the period table and the data tables and are kept in structs here; the module's
// bytes at their addresses are only their initial values.
//
// The replayer was edited from game to game. A build is recognised by the decoded code (wb_decode.h): its hash, with
// the absolute data operands zeroed, gives the build; the build's table gives which operand is which table or variable.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/wallybeben.h>

#define WB_MAX_MODULE_BYTES (4u << 20)
#define WB_MAX_SONGS 255u
#define WB_SONG_BYTES 16u // one song in the song table: the sequence list of each voice
#define WB_BAD_POINTER 0xFFFFFFFFu
#define WB_ZERO_STREAM 0xFFFFFFFEu // a pattern stream in the part of the address space that reads as zeros
#define WB_VOICE_RECORD_BYTES 0x84u
// UADE loads every module at this address; chip memory behind it is zero up to 2 MiB and there is no memory up to the
// CIAs: reads there give zeros.
#define WB_UADE_LOAD_ADDRESS 0xF000u
#define WB_UNMAPPED_HIGH 0xA00000u
// Paula's DMA reads chip memory, 2 MiB in UADE, and its addresses repeat every 2 MiB.
#define WB_CHIP_MASK 0x1FFFFFu
// The 68000 has 24 address lines: the replayer's addresses are taken modulo 16 MiB.
#define WB_ADDRESS_MASK 0xFFFFFFu

// What differs between the builds of the replayer.

extern const WbVersion wb_versions[];
extern const uint32_t wb_version_count;

typedef struct WbInstrument {
    uint8_t sample; // index into the sample table
    uint8_t flags;
    uint16_t length;      // bytes of the first block (flags 0)
    uint16_t loop_offset; // flags != 0: first block = loop_offset + loop_length
    uint16_t loop_length;
} WbInstrument;

typedef struct WbEnvelope {
    uint8_t volume;
    uint8_t target;
    uint8_t attack;
    uint8_t release;
    uint8_t sustain;
    uint8_t vibrato;
    uint8_t arp_mask;
    uint8_t flags;
} WbEnvelope;

// The voice records of the replayer: field f of voice v is byte f + v of the block, so each field is an array of four
// bytes.

_Static_assert(sizeof(WbVoiceBlock) == WB_VOICE_RECORD_BYTES, "the voice block is 0x84 bytes");

// The work an open may cost: the scans, the decoder, the patch search and the replay that proves the module safe all
// charge their steps to one budget; a module that exhausts it is refused.

#define WB_WORK_BUDGET (1ull << 30)

// Charges `units`; false (and nothing left) when the budget cannot pay.
bool wb_budget_take(WbBudget* budget, uint64_t units);

bool wb_module_load(WbModule* module, const uint8_t* data, size_t size, WbBudget* budget);
void wb_module_free(WbModule* module);

// Records of the instrument and envelope tables. They are addressed by a byte offset the song data supplies (up to 255
// into the table); false when the record is not module data the port models (it would be a variable or a pointer
// table).
bool wb_module_instrument(const WbModule* module, uint8_t wave, WbInstrument* out);
bool wb_module_envelope(const WbModule* module, uint8_t env_sel, WbEnvelope* out);
bool wb_module_arp_byte(const WbModule* module, uint32_t index, uint8_t* out);
// A note's period: `byte_index` is the offset into the period table (twice the note).
bool wb_module_period(const WbModule* module, uint32_t byte_index, uint16_t* out);
// The stream of pattern `number` (the lists name up to 0xC0): a module offset or WB_ZERO_STREAM; false when it lies in
// memory the port does not model.
bool wb_module_pattern(const WbModule* module, uint32_t number, uint32_t* stream);
// A byte of the module that the replayer reads as data (a pattern or sequence list); false outside the data.
bool wb_module_data_u8(const WbModule* module, uint32_t at, uint8_t* out);
