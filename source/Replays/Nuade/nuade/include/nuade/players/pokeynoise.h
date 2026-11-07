// SPDX-License-Identifier: MIT
// Pokeynoise player (pn.<name> + pn.<name>.info), translated from the UADE eagleplayer "Pokeynoise" (POKEYNoise
// V0.25alpha). A song is an Atari 8-bit binary (the 6502 code and data of a game's sound routine); the eagleplayer runs
// it on a 6502 interpreter with the 64 KB of an Atari, reads the POKEY registers the routine writes and plays each of
// the four POKEY channels as a prerecorded waveform on the Paula channel of the same number.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PN_VOICES 4

// The CIA clock: the timebase of the tick length.
#define PN_TIMEBASE_HZ 709379

#define PN_RAM_SIZE 0x10000u

typedef struct PnPlayer PnPlayer;

typedef struct PnTickResult {
    uint32_t length; // until the next tick, in 1 / PN_TIMEBASE_HZ seconds
} PnTickResult;

typedef struct PnVoiceState {
    int period; // AUDxPER of the voice (0 before the first tick, 256 for a silent voice)
    int volume;
} PnVoiceState;

typedef struct PnMachine {
    uint8_t ram[PN_RAM_SIZE];
    // The generator behind a read of RANDOM ($D20A) and the choices of the noises.
    uint32_t random_sum;
    uint32_t random_mix;
    // The scanline the next read of VCOUNT ($D40B) sees.
    uint16_t beam_line;
} PnMachine;

// SONGS=<last>[,<default>[,<first>]] as the subsong numbers the eagleplayer reports: first <= default <= last.
typedef struct PnSongRange {
    uint16_t first;
    uint16_t last;
    uint16_t default_song;
} PnSongRange;

// The voice record of the eagleplayer: the sound of one Paula channel, and what changed since it was written.
typedef struct PnVoice {
    NuadePaulaChannel paula;
    uint32_t wave;
    uint16_t length_words;
    uint16_t period;
    uint8_t volume;
    bool wave_changed;
    bool period_changed;
} PnVoice;

struct PnPlayer {
    uint8_t* song;
    size_t song_size;
    uint8_t* info;
    size_t info_size;
    PnSongRange range;
    bool rh_correction;
    uint8_t* waves; // pn_wave_data behind which the Paula channels can read
    PnMachine machine;
    uint16_t play_address;
    PnVoice voices[PN_VOICES];
};

// The eagleplayer's Check2 as far as the song data tells: an Atari binary (0xFFFF header) with the "pn" file name
// prefix. The ".info" file next to the song (TYPE=POKEY) is checked by `pn_create`.
bool pn_probe(const uint8_t* data, size_t size, const char* filename);

// Loads the song and its ".info" file (through `io`, next to `path`); null when either is missing or not a POKEY song.
PnPlayer* pn_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void pn_destroy(PnPlayer* player);

uint32_t pn_subsong_count(const PnPlayer* player);
uint32_t pn_default_subsong(const PnPlayer* player);

// (Re)start a subsong (0-based; the first one is the ".info" minimum); the player drives `voices` (Paula channel n =
// voice n).
bool pn_start(PnPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick (the eagleplayer's interrupt) and hand the Paula registers it wrote to the voices. The
// eagleplayer never calls SongEnd.
PnTickResult pn_tick(PnPlayer* player, NuadeVoices* voices);

PnVoiceState pn_voice_state(const PnPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PnPlayer* pn_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
