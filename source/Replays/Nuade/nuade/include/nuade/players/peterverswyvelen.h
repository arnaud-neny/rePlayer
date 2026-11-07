// SPDX-License-Identifier: MIT
// Peter Verswyvelen player ("Peter Verswyvelen Packer player module", the Uncle Tom songs), translated from the UADE
// eagleplayer "PeterVerswyvelen". A song is a ProTracker-like module of its own layout: 31 sample headers, a table of
// pattern offsets, a sequence of four pattern references per step and 16-row patterns of 4-byte notes that carry a
// repeat count. The replayer is in the eagleplayer.

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

#define PV_VOICES 4

// The CIA clock: the timebase of PV_TICK_LENGTH.
#define PV_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / PV_TIMEBASE_HZ seconds.
#define PV_TICK_LENGTH 14187

#define PV_SAMPLES 31

typedef struct PvPlayer PvPlayer;

typedef struct PvTickResult {
    bool looped; // the replayer called SongEnd: the sequence wrapped
} PvTickResult;

typedef struct PvVoiceState {
    int period;     // the period of the last note, 0 if none
    int instrument; // the last sample number loaded, -1 if none
} PvVoiceState;

typedef enum PvEffect {
    PvEffect_Arpeggio = 0,
    PvEffect_SlideUp = 1,
    PvEffect_SlideDown = 2,
    PvEffect_TonePortamento = 3,
    PvEffect_Vibrato = 4,
    PvEffect_VolumeSlide = 0xA,
    PvEffect_SequenceJump = 0xB,
    PvEffect_SetVolume = 0xC,
    PvEffect_PatternBreak = 0xD,
    PvEffect_Filter = 0xE,
    PvEffect_Speed = 0xF,
} PvEffect;

typedef struct PvLayout {
    uint32_t sequence;      // the sequence: four words per entry
    uint32_t patterns;      // the pattern data the pattern offset table points into
    uint32_t samples;       // the first sample
    int16_t sequence_bytes; // 8 per entry
    int16_t restart_bytes;  // where the sequence restarts: the restart entry times 8
} PvLayout;

typedef struct PvVoice {
    NuadePaulaChannel paula;
    int32_t note_ptr; // the next note in the pattern
    uint16_t period;  // the period of the row's note
    uint8_t sample;   // the last sample number loaded
    int8_t duration;  // rows the note still repeats; below 0 = read the next note
    bool sounding;    // the note plays
    PvEffect effect;  // the row's effect and its parameter
    uint8_t param;
    uint8_t param_low; // the parameter's nibbles
    uint8_t param_high;
    int32_t start; // the sample, where a note starts
    uint16_t length_words;
    uint16_t volume;
    int32_t loop_start; // what the DMA repeats
    uint16_t loop_words;
    uint16_t volume_row; // volume and period at the row's effect, restored on a repeated note
    uint16_t period_row;
    int16_t current_period;
    bool dma_start; // the row starts the DMA of the voice
    bool triggered; // a note has written AUDxLC / AUDxLEN
    bool porta_down;
    uint8_t porta_speed;
    uint16_t porta_target;
    uint8_t vibrato_speed;
    uint8_t vibrato_depth;
    uint8_t vibrato_pos;
    int last_period;
    int last_sample;
} PvVoice;

struct PvPlayer {
    uint8_t* mem; // the module, padded for Paula
    size_t size;
    PvLayout layout;
    uint32_t sample_start[PV_SAMPLES];
    uint8_t speed;
    uint8_t speed_counter;
    int16_t position; // byte offset of the next sequence entry
    int16_t row_counter;
    bool fault; // the replayer left what the port models (dry run only)
    bool looped;
    PvVoice voices[PV_VOICES];
};

// The eagleplayer's Check2 and the size test of its InitPlayer, plus the limits of what this port models.
bool pv_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `pv_probe` rejects it.
PvPlayer* pv_create(const uint8_t* data, size_t size);
void pv_destroy(PvPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool pv_start(PvPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
PvTickResult pv_tick(PvPlayer* player, NuadeVoices* voices);

// Ticks per row the song is at.
uint32_t pv_grid_ticks(const PvPlayer* player);

PvVoiceState pv_voice_state(const PvPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PvPlayer* pv_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
