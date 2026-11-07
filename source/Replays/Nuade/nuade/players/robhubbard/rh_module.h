// SPDX-License-Identifier: MIT
// A Rob Hubbard module as the UADE eagleplayer opens it, and the build of the replayer that it carries.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/robhubbard.h>

#define RH_SUBSONG_RECORD_SIZE 0x12 // a zero byte, the tempo and one track list for each voice
#define RH_VIBRATO_RESTART 0x84     // a vibrato wave byte that sends the vibrato back to the start of its wave
#define RH_COMMANDS_PER_TICK 4096   // the most commands the port lets a voice read without reaching a note

// Opens a module like the eagleplayer does and checks that this port plays it exactly. null if not.
RhModule* rh_module_open(const uint8_t* data, size_t size);
void rh_module_close(RhModule* module);

// The module as the replayer's init leaves it before a subsong starts: the instrument records filled in from the
// sample headers.
void rh_module_reset(RhModule* module);

// Reads outside the module give 0, writes outside it are dropped.
uint8_t rh_module_u8(const RhModule* module, uint32_t offset);
uint16_t rh_module_u16(const RhModule* module, uint32_t offset);
uint32_t rh_module_u32(const RhModule* module, uint32_t offset);
void rh_module_store_u8(RhModule* module, uint32_t offset, uint8_t value);

// An instrument record: a sample pointer, the repeat, its length, the base period, the volume, and (extended builds)
// the vibrato divisor and wave, and the ferris wheel's two limits.
typedef struct RhInstrument {
    uint32_t sample;
    int32_t repeat; // offset of the repeat from the sample start in bytes; 0: the repeat is silence
    uint16_t words; // length of the sample in words
    uint16_t base_period;
    uint16_t volume;
    uint16_t vibrato_divisor; // 0: no vibrato
    int16_t wave;             // offset of the vibrato wave from the wave table
    uint16_t ferris_max;      // 0: no ferris wheel
    uint16_t ferris_min;
} RhInstrument;

// The offset of instrument record `index`.
uint32_t rh_module_instrument(const RhModule* module, uint32_t index);

RhInstrument rh_module_read_instrument(const RhModule* module, uint32_t record);

// A subsong record: the tempo (ticks per unit of note length) and the offset of the track list of each voice.
typedef struct RhSubsong {
    uint8_t tempo;
    uint32_t track_list[RH_VOICES];
} RhSubsong;

RhSubsong rh_module_read_subsong(const RhModule* module, uint32_t subsong);

// The subsong record the init plays when asked for `subsong`: it takes the low byte of the number, and some builds
// always take 0.
uint32_t rh_module_played_subsong(const RhModule* module, uint32_t subsong);

// Number of entries in a track list before its terminating 0.
uint32_t rh_module_track_entries(const RhModule* module, uint32_t list);

// The instrument an operand of the instrument command selects: the eagleplayer's InitSound leaves the entry count of
// voice 3's track list of the subsong it was asked for in a register, and some builds use it for an operand with bit 7.
uint32_t rh_module_resolve_instrument(const RhModule* module, uint32_t subsong, uint8_t operand);
