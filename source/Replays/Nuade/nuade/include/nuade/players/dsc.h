// SPDX-License-Identifier: MIT
// Digital Sonix & Chrome player (Andrew E. Bailey & David M. Hanlon, 1990), translated from the UADE eagleplayer
// (Wanted Team).

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

#define DSC_VOICES 4

// The CIA clock: the timebase of DSC_TICK_LENGTH.
#define DSC_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / DSC_TIMEBASE_HZ seconds.
#define DSC_TICK_LENGTH 14187

typedef struct DscPlayer DscPlayer;

typedef struct DscTickResult {
    bool looped; // the subsong wrapped to its first entry during this tick
} DscTickResult;

typedef struct DscVoiceState {
    int sound; // the sound last started on the voice, -1 if none
} DscVoiceState;

typedef struct DscModule {
    const uint8_t* data; // the module bytes (DscPlayer owns them)
    uint32_t size;
    int16_t tempo;
    uint8_t sound_count;
    uint8_t entry_count;
    uint32_t sample_size;
    uint32_t rows; // length of one voice column
    uint32_t entries;
    uint32_t lines; // the four columns
    uint32_t sounds;
    uint32_t samples;
    int subsong_count;
} DscModule;

typedef struct DscVoice {
    int sound;
    uint16_t blocks_left; // the interrupt handler's counter: blocks still to chain before the silence
    bool irq_enabled;
    const int8_t* loop;
    uint16_t loop_words;
    uint32_t wraps_seen; // the mixer's block starts already answered

    NuadePaulaChannel paula;
} DscVoice;

typedef struct DscSong {
    uint8_t tempo;
    uint8_t tempo_counter;
    uint32_t first_entry;
    uint32_t entry;
    uint32_t line;
    uint32_t row;
    uint32_t repeats;
    bool looped;
    DscVoice voices[DSC_VOICES];
} DscSong;

struct DscPlayer {
    DscModule module;
    uint8_t* data; // owned copy of the module and NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes (nuade_paula_padded_copy)
    DscSong song;
};

// The eagleplayer's structure check (header counts and sizes, song table offsets, sound table against the sample
// data size).
bool dsc_probe(const uint8_t* data, size_t size);

// 0 when the data is not a Digital Sonix & Chrome module (or out of memory).
DscPlayer* dsc_create(const uint8_t* data, size_t size);
void dsc_destroy(DscPlayer* player);

int dsc_subsong_count(const DscPlayer* player);

// (Re)start subsong `subsong` (0-based). The player drives `voices` (Paula channel n = voice n).
bool dsc_start(DscPlayer* player, int subsong, NuadeVoices* voices);

DscTickResult dsc_tick(DscPlayer* player, NuadeVoices* voices);

// Ticks per song line.
int dsc_tempo(const DscPlayer* player);

DscVoiceState dsc_voice_state(const DscPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
DscPlayer* dsc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
