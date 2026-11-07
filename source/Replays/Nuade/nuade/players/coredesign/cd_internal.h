// SPDX-License-Identifier: MIT
// Core Design player: types shared by the module loader and the driver.
//
// A module is one game's sound driver as an AmigaDOS executable: a single hunk that starts with `moveq #-1,d0; rts`,
// "S.PHIPPS" and a table of the driver's entry points and data, followed by the driver's code, its variables, the
// tables (songs, patterns, instruments, periods) and the samples. The eagleplayer loads it, patches the driver and
// calls it. Here the hunk is kept as loaded (offsets into it are the driver's addresses), the variables are structs,
// and the tables are read from the hunk at the offsets that the loader finds in the driver's code.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/coredesign.h>

// The hunk: a read past it returns 0 (the driver would read whatever follows in memory). The allocation comes from
// nuade_paula_padded_copy, so a sample that starts inside the hunk can run past its end.

uint8_t cd_u8(const CdMemory* m, uint32_t offset);
uint16_t cd_u16(const CdMemory* m, uint32_t offset);
int16_t cd_s16(const CdMemory* m, uint32_t offset);
uint32_t cd_u32(const CdMemory* m, uint32_t offset);

// The module's entry points and data, from the table after "S.PHIPPS" (all offsets into the hunk except the counts).

#define CD_INSTRUMENT_SIZE 14

// The tables the code addresses; all are offsets into the hunk.

#define CD_SUBSONG_SIZE 8
#define CD_SONG_SIZE 6
#define CD_VIBRATO_SIZE 8

// A subsong: a song to play, or a sound effect (one instrument at one period).
typedef struct CdSubsong {
    bool sound_effect;
    uint16_t index; // the song, or the instrument
    uint16_t period;
    uint16_t param; // sound effect: the sustain time or the voice selection (CdVersion)
} CdSubsong;

// An instrument record. The envelope rates are added to an 8 bit volume.
typedef struct CdInstrument {
    int8_t attack_rate;
    int8_t decay_rate;
    int8_t sustain_level;
    int8_t release_rate;
    int8_t peak;
    bool one_shot;   // flag bit 0: play the sample once, then a silent buffer
    bool has_attack; // flag bit 1, see CdVersion.attack_flag
    uint32_t sample; // a word with the length in words, then the data
    int16_t loop_offset;
    uint16_t loop_words;
} CdInstrument;

// A vibrato: a control byte and the sequence of steps that follows it. Each step is a note offset (bits 3 to 7) and a
// delay in ticks (bits 0 to 2).
typedef struct CdVibrato {
    bool restart;      // control bit 7: play from the start of the sequence
    uint8_t delay;     // ticks before the first step
    uint8_t steps;     // steps in the sequence
    uint32_t sequence; // offset of the first step
} CdVibrato;

CdSubsong cd_read_subsong(const CdMemory* m, const CdTables* tables, uint32_t index);
CdInstrument cd_read_instrument(const CdMemory* m, uint32_t offset);
CdVibrato cd_read_vibrato(const CdMemory* m, const CdTables* tables, uint8_t number);

// The order list of one track of a song.
uint32_t cd_read_song_track(const CdMemory* m, const CdTables* tables, uint8_t song, int track);
// The first byte of a pattern.
uint32_t cd_read_pattern(const CdMemory* m, const CdTables* tables, uint8_t pattern);
// The period of a note, a signed byte that indexes the period table. Past the table's end it reads the silent buffer
// (period 0).
uint16_t cd_read_period(const CdMemory* m, const CdTables* tables, uint8_t note);

// The loaded module

// Loads the executable, finds the tables and the version. This is also the check: false for anything the player
// cannot play.
bool cd_module_load(CdModule* module, const uint8_t* data, size_t size);
void cd_module_free(CdModule* module);

// Driver

void cd_driver_start(CdPlayer* player, int subsong);
void cd_driver_tick(CdPlayer* player);
