// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GmcModule GmcModule;

typedef struct GmcPosition {
    int position; // index in the position list
    int row;
    int speed; // ticks per row
} GmcPosition;

typedef struct GmcVoiceState {
    uint16_t period; // of the last note started on the channel, 0 if none
    uint8_t sample;  // its sample number (1..15), 0 if none
} GmcVoiceState;

typedef struct GmcCell {
    uint16_t period;
    uint8_t sample;
    uint8_t effect;
    uint8_t effect_arg;
} GmcCell;

typedef enum GmcEffect {
    GMC_EFFECT_NONE = 0,
    GMC_EFFECT_SLIDE_UP = 1,
    GMC_EFFECT_SLIDE_DOWN = 2,
    GMC_EFFECT_SET_VOLUME = 3,
    GMC_EFFECT_PATTERN_BREAK = 4,
    GMC_EFFECT_POSITION_JUMP = 5,
    GMC_EFFECT_ENABLE_FILTER = 6,
    GMC_EFFECT_DISABLE_FILTER = 7,
    GMC_EFFECT_SET_SPEED = 8
} GmcEffect;

typedef struct GmcTrackLine {
    uint16_t period;
    uint8_t sample;
    GmcEffect effect;
    uint8_t effect_arg;
} GmcTrackLine;

typedef struct GmcPattern {
    GmcTrackLine tracks[4][64];
} GmcPattern;

typedef struct GmcSample {
    int8_t* data;
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
    uint16_t volume;
} GmcSample;

typedef struct GmcChannelInfo {
    int32_t slide;
    uint16_t period;
    uint16_t volume;
    uint16_t started_period; // period and sample number of the last note started on the channel
    uint8_t started_sample;
} GmcChannelInfo;

typedef struct GmcGlobalPlayingInfo {
    uint16_t song_speed;
    uint16_t song_step;
    uint16_t pattern_count;
    int16_t current_position;
    uint8_t current_pattern;

    uint16_t shown_position; // the position and row play_pattern_row played last
    uint16_t shown_row;
} GmcGlobalPlayingInfo;

// What the tick wrote to a Paula channel; handed to the voice at the end of the tick.
typedef struct GmcChannel {
    const int8_t* sample_data;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;
    bool active;
    bool play_pending;
    bool muted;
} GmcChannel;

struct GmcModule {
    NuadeVoices* voices;

    // Module data
    int num_positions;
    uint8_t* position_list;
    GmcSample samples[15];
    GmcPattern* patterns;
    int num_patterns;

    // Playing state
    GmcGlobalPlayingInfo playing_info;
    GmcChannelInfo channel_info[4];
    uint32_t loop_count;

    // Position visited tracking
    uint8_t visited[256];

    GmcChannel channels[4];
};

// True when the data is laid out like a Game Music Creator module (the format has no mark; the sample table, the
// position list and the size are checked).
bool gmc_probe(const uint8_t* data, size_t size);

GmcModule* gmc_create(const uint8_t* data, size_t size);
void gmc_destroy(GmcModule* module);

int gmc_subsong_count(const GmcModule* module);
// (Re)start from the first position, playing through the given voices.
bool gmc_start(GmcModule* module, int subsong, NuadeVoices* voices);

int gmc_channel_count(const GmcModule* module);
void gmc_set_channel_mask(GmcModule* module, uint32_t mask);

// One 50 Hz player tick; the voices carry the result. True when the song passed its loop point during the tick.
bool gmc_tick(GmcModule* module);

// The row whose notes were started by the last tick.
GmcPosition gmc_position(const GmcModule* module);
int gmc_position_count(const GmcModule* module);

GmcVoiceState gmc_voice_state(const GmcModule* module, int channel);

// `position` 0 .. gmc_position_count - 1, `channel` 0..3, `row` 0..63.
GmcCell gmc_pattern_cell(const GmcModule* module, int position, int channel, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
GmcModule* gmc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
