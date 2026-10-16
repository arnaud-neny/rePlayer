// PreTracker C player — C port of the Raspberry Casket Amiga music replayer
// Copyright (c) 2022 Chris 'platon42' Hodges <chrisly@platon42.de>
// Copyright (c) 2026 Daniel Collin <daniel@collin.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PRE_MAX_WAVES 24
#define PRE_MAX_INSTRUMENTS 32
#define PRE_NAME_MAX_LEN 24 // 23 chars + null terminator

typedef struct PreSongMetadata {
    char song_name[21];
    char author[21];
    uint8_t num_waves;
    uint8_t num_steps;
    uint16_t num_positions;
    uint8_t num_subsongs;
    uint8_t num_instruments;
    char wave_names[PRE_MAX_WAVES][PRE_NAME_MAX_LEN];
    char instrument_names[PRE_MAX_INSTRUMENTS][PRE_NAME_MAX_LEN];
} PreSongMetadata;

// A single decoded track cell (unpacked from the 3-byte packed format).
// Tracks are single-channel sequences of rows. The position table assigns
// a track (with optional pitch shift) to each channel at each position.
typedef struct PreTrackCell {
    uint8_t note;        // 0=none, 1-60=chromatic note, 0x3D=note off
    uint8_t instrument;  // 0=none, 1-32
    bool has_arpeggio;   // true if arpeggio flag set (effect bytes are arp notes, not an effect)
    uint8_t effect_cmd;  // 0x00-0x0F (0 when has_arpeggio is true)
    uint8_t effect_data; // raw effect parameter (arp notes when has_arpeggio is true)
} PreTrackCell;

// Live playback state snapshot, updated each tick during decode.
typedef struct PrePlaybackState {
    uint16_t position;       // current position index
    uint8_t row;             // current row within track (0 to num_steps-1)
    uint8_t speed;           // current ticks-per-row
    uint8_t ticks_remaining; // countdown to next row
    struct {
        uint8_t track_num;   // track assigned at this position (0=empty/muted)
        int8_t pitch_shift;  // transpose applied to this track
        uint8_t instrument;  // current instrument number (0=none)
        uint8_t volume;      // pattern volume (0-64)
        uint8_t wave;        // current wave number
        uint8_t adsr_phase;  // 0=attack, 1=decay, 2=sustain, 3=release
    } channels[4];
} PrePlaybackState;

typedef enum {
    PRE_INTERP_BLEP = 0, // Nearest-neighbor + BLEP (default, matches PreTracker.exe)
    PRE_INTERP_SINC = 1, // Windowed sinc interpolation (cleaner for HQ buffers)
} PreInterpMode;

// Lifecycle
struct PreSong* pre_song_create(const uint8_t* data, uint32_t size);
void pre_song_destroy(struct PreSong* song);

// Configuration (call before pre_song_start, or between restarts)
void pre_song_set_subsong(struct PreSong* song, int subsong);
void pre_song_set_sample_rate(struct PreSong* song, uint32_t rate);
void pre_song_set_solo_channel(struct PreSong* song, int32_t channel);
void pre_song_set_stereo_mix(struct PreSong* song, float mix);
void pre_song_set_interp_mode(struct PreSong* song, PreInterpMode mode);
void pre_song_set_stereo_width(struct PreSong* song, float delay_ms);

// Playback
void pre_song_start(struct PreSong* song);
int pre_song_decode(struct PreSong* song, float* buffer, int num_frames);
int pre_song_decode_with_scopes(struct PreSong* song, float* buffer, int num_frames, float** scopes, int num_scopes);
int pre_song_decode_muted(struct PreSong* song, int num_frames); // rePlayer
bool pre_song_is_finished(const struct PreSong* song);

// Song info
const PreSongMetadata* pre_song_get_metadata(const struct PreSong* song);
int pre_song_get_length(struct PreSong* song); // rePlayer

// Track data queries
bool pre_song_get_position_entry(const struct PreSong* song, uint16_t position, uint8_t channel,
                                 uint8_t* track_num, int8_t* pitch_shift);
bool pre_song_get_track_cell(const struct PreSong* song, uint8_t track, uint8_t row, PreTrackCell* cell);

// Live playback state (updated each tick during decode)
const PrePlaybackState* pre_song_get_playback_state(const struct PreSong* song);
