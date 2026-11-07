// SPDX-License-Identifier: MIT
// Kim Christensen player, translated from the UADE eagleplayer "Kim_Christensen" (Wanted Team). A song is the data
// block of a 1989 game: the eagleplayer finds the addresses of the order list, the patterns, the instruments and the
// sample table in the instructions that load them, and plays the song with its own replayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KIM_VOICES 4

// The CIA clock: the timebase of KIM_TICK_LENGTH.
#define KIM_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / KIM_TIMEBASE_HZ seconds.
#define KIM_TICK_LENGTH 14187

#define KIM_VOLUME_STAGES 4
#define KIM_PITCH_STAGES 2

typedef struct KimPlayer KimPlayer;

typedef struct KimTickResult {
    bool looped; // the order list ran past its last position: the eagleplayer calls SongEnd and starts over
} KimTickResult;

typedef struct KimVoiceState {
    int period;     // the current period (after the slides and the vibrato), 0 if none
    int instrument; // the instrument number last started, -1 if none
} KimVoiceState;

typedef struct KimModule {
    const uint8_t* data;
    size_t size;
    uint32_t origin;       // the address the game's code gives the first byte of the data
    uint32_t order_list;   // one pattern number per position, 0xFF = skip
    uint32_t patterns;     // 0x84 bytes each
    uint32_t instruments;  // 0x20 bytes each
    uint32_t sample_table; // 6 bytes each
    uint32_t silence;      // four zero-ish bytes the voices play after a sample that does not loop
    uint8_t speed;         // ticks per row minus one
    uint8_t first_position;
    uint8_t last_position;
} KimModule;

typedef struct KimVolumeRamp {
    uint8_t ticks;
    int16_t delta;
} KimVolumeRamp;

typedef struct KimPitchRamp {
    uint8_t hold;
    uint8_t ticks;
    int16_t delta;
} KimPitchRamp;

typedef struct KimVibrato {
    uint8_t delay;
    uint8_t ticks_left; // until the direction changes
    uint8_t ticks_per_half;
    int16_t delta;
} KimVibrato;

typedef struct KimVoice {
    NuadePaulaChannel paula;
    uint32_t start; // AUDxLC of the note
    uint16_t length_words;
    uint32_t loop; // what the sample repeats
    uint16_t loop_words;
    bool loops;
    bool started;   // the voice got a note in the current row: DMA goes on for it
    int16_t volume; // times 256
    int16_t period; // times 16
    KimVolumeRamp volume_ramps[KIM_VOLUME_STAGES];
    KimPitchRamp pitch_ramps[KIM_PITCH_STAGES];
    KimVibrato vibrato;
    int instrument; // for the display
} KimVoice;

struct KimPlayer {
    uint8_t* chip; // the data, padded for the Paula channels
    KimModule module;
    uint8_t position;
    uint8_t row;
    int row_countdown; // a signed byte (-128..127): ticks to the next row less one; a row starts below zero
    bool fault;        // the data led out of what the port models
    KimVoice voices[KIM_VOICES];
};

// Copies and parses the song: the eagleplayer's Check2 and InitPlayer on the data, plus what this port needs (every
// note, instrument and sample the song plays lies inside the data and the period table). Null when one refuses it.
KimPlayer* kim_create(const uint8_t* data, size_t size);
void kim_destroy(KimPlayer* player);

// (Re)start the song from its first position; the player drives `voices` (Paula channel n = voice n).
bool kim_start(KimPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
KimTickResult kim_tick(KimPlayer* player, NuadeVoices* voices);

// Ticks per row the song starts with.
uint32_t kim_grid_ticks(const KimPlayer* player);

KimVoiceState kim_voice_state(const KimPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
KimPlayer* kim_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
