// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)

#pragma once

// Art of Noise (AoN) Replayer - C11 Port
// =======================================
// Ported from NostalgicPlayer C# implementation.
// Supports AON4 (4-channel) and AON8 (8-channel) formats.
//
// Usage:
//   AonSong* song = aon_song_create(data, size);
//   aon_song_set_sample_rate(song, 48000);
//   aon_song_start(song);
//   while (!aon_song_is_finished(song)) {
//       int frames = aon_song_decode(song, buffer, 4096);
//   }
//   aon_song_destroy(song);

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AON_MAX_CHANNELS 8
#define AON_ROWS_PER_PATTERN 64
#define AON_MAX_ARPEGGIOS 16
#define AON_ARPEGGIO_LENGTH 4

#define AON_SCOPE_BUFFER_SIZE 1024
#define AON_SCOPE_BUFFER_MASK (AON_SCOPE_BUFFER_SIZE - 1)

typedef struct AonSong AonSong;

typedef struct AonSongMetadata {
    char song_name[256];
    char author[256];
    uint8_t num_positions;
    uint8_t num_patterns;
    uint8_t num_instruments;
    uint8_t num_channels;  // 4 (AON4) or 8 (AON8)
    uint8_t num_waveforms;
} AonSongMetadata;

// Lifecycle
AonSong* aon_song_create(const uint8_t* data, uint32_t size);
void aon_song_destroy(AonSong* song);

// Configuration (call before start)
void aon_song_set_subsong(AonSong* song, int subsong);
void aon_song_set_sample_rate(AonSong* song, uint32_t rate);
void aon_song_set_solo_channel(AonSong* song, int32_t channel);
void aon_song_set_stereo_mix(AonSong* song, float mix);

// Playback
void aon_song_start(AonSong* song);
int aon_song_decode(AonSong* song, float* buffer, int num_frames);
int aon_song_decode_muted(AonSong* song, int num_frames);
bool aon_song_is_finished(const AonSong* song);
int aon_song_get_length(AonSong* song); // rePlayer

// Metadata
const AonSongMetadata* aon_song_get_metadata(const AonSong* song);

// Returns instrument name for index (0-based), or NULL if out of range
const char* aon_song_get_instrument_name(const AonSong* song, uint8_t index);

// Pattern data types
typedef struct AonPatternCell {
    uint8_t note;        // 0=none, 1-63=chromatic note
    uint8_t instrument;  // 0=none, 1-63
    uint8_t arpeggio;    // 0-15, index into ARPG table
    uint8_t effect;      // 0-33 effect command
    uint8_t effect_arg;  // raw effect parameter byte
} AonPatternCell;

typedef struct AonPlaybackState {
    uint8_t position;        // current position index
    uint8_t pattern;         // current pattern number
    uint8_t row;             // current row (0-63)
    uint8_t speed;           // ticks per row
    uint8_t tempo;           // BPM (32-255)
    uint8_t ticks_remaining; // countdown to next row
    struct {
        uint8_t instrument;     // 0=none, 1-based
        uint8_t volume;         // pattern volume (0-64)
        uint8_t synth_volume;   // envelope volume (0-127)
        uint8_t track_volume;   // track volume (0-64)
        uint16_t period;        // Amiga period (after slides/arpeggio)
        uint8_t effect;         // current effect command
        uint8_t effect_arg;     // current effect argument
        uint8_t envelope_phase; // 0=done, 1=add, 2=sub
    } channels[AON_MAX_CHANNELS];
} AonPlaybackState;

// Pattern data queries
bool aon_song_get_pattern_cell(const AonSong* song, uint8_t pattern, uint8_t row,
                                uint8_t channel, AonPatternCell* cell);
bool aon_song_get_position_pattern(const AonSong* song, uint8_t position, uint8_t* pattern);
bool aon_song_get_arpeggio(const AonSong* song, uint8_t index, uint8_t values[AON_ARPEGGIO_LENGTH]);
const AonPlaybackState* aon_song_get_playback_state(const AonSong* song);

// Scope visualization
void aon_song_enable_scope_capture(AonSong* song, int enable);
uint32_t aon_song_get_scope_data(AonSong* song, int channel, float* buffer, uint32_t num_samples);

#ifdef __cplusplus
}
#endif
