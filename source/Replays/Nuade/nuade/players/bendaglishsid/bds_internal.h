// SPDX-License-Identifier: MIT
// Ben Daglish game sound driver: types shared by the module loader and the driver.
//
// A module is one game's sound driver as an AmigaDOS executable: a single hunk that starts with `moveq #-1,d0; rts`,
// "DAGLISH!" and a table of the driver's entry points and strings, followed by the driver's code, its variables, the
// tables (order lists, tracks, instruments, wave forms) and the samples. The eagleplayer loads it, patches it and calls
// it. Here the hunk is kept as loaded (offsets into it are the driver's addresses), the variables are structs, and the
// tables are read from the hunk at the offsets that the loader finds in the driver's code.
//
// Two revisions of the driver exist in the corpus. The early one (Terramex) plays wave forms only and sets the volume
// of a voice from the audio interrupt; the late one (Chubby Gristle, The Flintstones) adds one-shot sample
// instruments, a volume cap and several subsongs.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/bendaglishsid.h>

#define BDS_MAX_SUBSONGS 256

// The hunk: a read past it returns 0 (the driver would read whatever follows in memory), a write past it is dropped.
// The allocation comes from nuade_paula_padded_copy, so a sample that starts inside the hunk can run past its end.

uint8_t bds_u8(const BdsMemory* m, uint32_t offset);
uint16_t bds_u16(const BdsMemory* m, uint32_t offset);
uint32_t bds_u32(const BdsMemory* m, uint32_t offset);
void bds_put_u8(BdsMemory* m, uint32_t offset, uint8_t value);

// The tables the code addresses; all are offsets into the hunk.

#define BDS_MAP_SLOTS 24 // instrument map entries a song starts with

// An instrument record. The envelope rates are nibbles that index the rate tables.
typedef struct BdsInstrument {
    uint8_t wave_flags;     // bits 4 to 6 select the wave pointer table
    bool muted;             // early: flag bit 7, the voice plays the silent buffer
    bool is_sample;         // late: flag bit 7, the instrument is a sample
    uint8_t sample_index;   // late: flag bits 1 to 6, the index of the sample's record
    uint8_t attack_nibble;  // byte 3, high nibble
    uint8_t decay_nibble;   // byte 3, low nibble
    uint8_t sustain_level;  // byte 4, high nibble
    uint8_t release_nibble; // byte 4, low nibble
    uint8_t gate_ticks;     // byte 5: the note's release starts this many ticks before it ends
    uint8_t vibrato;        // byte 7: vibrato table index, 0 = none
    uint8_t volume_max;     // byte 8
} BdsInstrument;

// A sample instrument's record (late driver).
typedef struct BdsSample {
    uint32_t lc;
    uint16_t len_words;
    uint16_t period;
} BdsSample;

BdsInstrument bds_read_instrument(const BdsMemory* m, uint32_t offset);
BdsSample bds_read_sample(const BdsMemory* m, uint32_t offset);

// The loaded module

// Loads the executable, finds the tables and the revision. This is also the check: false for anything the player
// cannot play.
bool bds_module_load(BdsModule* module, const uint8_t* data, size_t size);
void bds_module_free(BdsModule* module);

// Driver

void bds_driver_start(BdsPlayer* player, int subsong);
void bds_driver_tick(BdsPlayer* player);
// Hand the registers of the tick to the voices of the mixer.
void bds_driver_sync(BdsPlayer* player);
