// SPDX-License-Identifier: MIT
// Desire player: types shared by the module loader and the replayer.
//
// A module is raw 68k memory: a data block that holds the voices' variables (the header, 16 bytes per voice and the
// fields of all voices interleaved), the replayer code at the end of it, then the replayer's tables (periods, sequence
// and instrument tables) and the music. The eagleplayer finds the code by its instructions and calls it once per tick.
// Here the module is kept as loaded (offsets into it are the replayer's addresses), the voices' variables are structs
// that start from the header's values, and the tables are read from the module at the offsets that the loader finds in
// the code.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/desire.h>

#define DSR_EFFECT_VOICE DSR_VOICES // the fifth record: a sound effect that borrows Paula channel 3
#define DSR_MAX_SUBSONGS 63         // the subsong number goes into a voice's state byte, below its bit 6
#define DSR_PERIODS 96              // the period table: 8 steps per semitone, 12 semitones

// The module, zero filled beyond its end: a read past it returns 0 (the replayer would read whatever follows in
// memory). An AUDxLC up to `capacity` has room for any AUDxLEN (nuade_paula_padded_copy of `capacity` bytes).

uint8_t dsr_u8(const DsrMemory* m, int64_t offset);
uint16_t dsr_u16(const DsrMemory* m, int64_t offset);
int16_t dsr_s16(const DsrMemory* m, int64_t offset);
uint32_t dsr_u32(const DsrMemory* m, int64_t offset);

// The tables the code addresses; all are offsets into the module.

#define DSR_ORDER_ENTRY 2 // bytes

#define DSR_COMMAND_ARGS 5 // the longest command, the envelope

// The bytes that follow a command byte at `at`; each command uses the first few.
typedef struct DsrCommandArgs {
    uint8_t byte[DSR_COMMAND_ARGS];
} DsrCommandArgs;

typedef struct DsrInstrument {
    uint32_t sample; // offset of the first sample byte that Paula plays
    uint16_t length; // AUDxLEN in words
    uint8_t transpose;
} DsrInstrument;

// The loaded module

// Finds the replayer in the module's code and the tables. This is also the check: false for anything the player
// cannot play. The module's memory points at `data`.
bool dsr_module_locate(DsrModule* module, const uint8_t* data, size_t size);
// Locates the replayer, then makes the zero padded copy that the sample pointers need.
DsrStatus dsr_module_load(DsrModule* module, const uint8_t* data, size_t size);
void dsr_module_free(DsrModule* module);

// Voice records in the module's data block.
int dsr_record_count(const DsrVersion* version);

// Table readers. The index is the 16 bit register of the replayer; the song list, the order table and the modulation
// table add it sign extended to the table address.
uint8_t dsr_read_song_list(const DsrModule* module, uint16_t index);
// The offset of a track: the entry at byte position `at` of the order table.
uint16_t dsr_read_order(const DsrModule* module, uint16_t at);
int8_t dsr_read_modulation(const DsrModule* module, uint16_t index);
uint16_t dsr_read_period(const DsrModule* module, uint16_t index);
DsrInstrument dsr_read_instrument(const DsrModule* module, uint8_t number);
DsrCommandArgs dsr_read_command_args(const DsrModule* module, uint16_t at);

// The replayer's state

// Decodes the state byte, or the request byte of a record (the same coding: bit 6 selects the song list).
DsrMode dsr_decode_mode(uint8_t state);
DsrRequest dsr_decode_request(uint8_t request);

typedef enum DsrEnvelope {
    DsrEnvelope_Release = 0,
    DsrEnvelope_Attack = 1,
    DsrEnvelope_Hold = 2,
    DsrEnvelope_Decay = 3,
    DsrEnvelope_Sustain = 4,
} DsrEnvelope;

// A voice record as the module's data block holds it at the start.
DsrVoice dsr_read_voice(const DsrModule* module, int voice);

void dsr_replayer_start(DsrPlayer* player, int subsong);
void dsr_replayer_tick(DsrPlayer* player);
