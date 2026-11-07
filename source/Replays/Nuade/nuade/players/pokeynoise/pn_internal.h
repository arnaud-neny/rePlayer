// SPDX-License-Identifier: MIT
// Pokeynoise: the Atari machine (64 KB and the 6502 interpreter), the ".info" file lookups, the mapping from POKEY
// registers to Paula and the waveforms it plays.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/pokeynoise.h>

// The Atari: memory and the 6502 interpreter (pn_cpu.c)

// The POKEY registers the player reads, in the page the Atari maps POKEY to; AUDFn / AUDCn are at 2 * (n - 1) and
// 2 * (n - 1) + 1.
#define PN_POKEY_BASE 0xD200u
#define PN_POKEY_AUDCTL 8u

// The immediate vertical blank vector of the operating system: the play routine.
#define PN_VVBLKI 0x222u

typedef enum PnRunResult {
    PnRun_Done,  // the routine returned (or left through the operating system's vertical blank exit)
    PnRun_Fault, // an opcode the interpreter does not know, a stack over- or underflow, or a runaway routine
} PnRunResult;

// The operating system's addresses a song can call: SETVBV, SYSVBV and XITVBV.
#define PN_OS_SETVBV 0xE45Cu
#define PN_OS_SYSVBV 0xE45Fu
#define PN_OS_XITVBV 0xE462u

// Memory with the generator in its initial state and nothing loaded.
void pn_machine_reset(PnMachine* machine);

// Load an Atari binary: segments of (first address, last address, bytes), each optionally behind a 0xFFFF marker.
void pn_machine_load(PnMachine* machine, const uint8_t* file, size_t size);

// Run the routine at `pc` with the given registers until it returns. A call of SETVBV sets the vector in memory.
PnRunResult pn_machine_run(PnMachine* machine, uint16_t pc, uint8_t a, uint8_t x, uint8_t y);

uint16_t pn_machine_u16(const PnMachine* machine, uint16_t address);

// The next byte of the generator (RANDOM reads a different byte of the same step).
uint8_t pn_machine_random_byte(PnMachine* machine);

// The ".info" file (pn_info.c)

typedef struct PnInfo {
    const uint8_t* data;
    size_t size;
} PnInfo;

// The text after a tool type key. The eagleplayer's icon.library finds a key as a plain substring of the file and its
// value starts one character behind it; past the end of the file the text is zeros.
typedef struct PnToolType {
    bool found;
    const uint8_t* data;
    size_t size;
    size_t offset;
} PnToolType;

PnToolType pn_info_find(const PnInfo* info, const char* key);

// The value is exactly `text` (compared with its terminating zero).
bool pn_tool_type_is(const PnToolType* tool, const char* text);

PnSongRange pn_info_song_range(const PnInfo* info);

// POKEY to Paula (pn_pokey.c)

// Waveforms: the data hunk of the eagleplayer (pn_waves.c). A wave is a byte offset into it and a length in words.
#define PN_WAVE_BYTES 0xBF0Cu

// The silent waveform: the first words of the table. A voice record starts as this waveform.
#define PN_SILENT_WAVE 0u
#define PN_SILENT_WORDS 4u
extern const int8_t pn_wave_data[PN_WAVE_BYTES];

// What one POKEY channel plays: the waveform, its period and volume.
typedef struct PnSound {
    uint32_t wave; // byte offset into pn_wave_data
    uint16_t length_words;
    uint16_t period;
    uint8_t volume;
} PnSound;

// The sound of POKEY channel `channel` (0..3) for the registers now in memory. `rh_correction` is the AUDIOCORRECT=RH
// setting of the ".info" file.
PnSound pn_pokey_sound(PnMachine* machine, unsigned channel, bool rh_correction);
