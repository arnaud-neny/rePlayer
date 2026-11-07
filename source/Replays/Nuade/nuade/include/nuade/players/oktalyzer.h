// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)

#pragma once

#include <nuade/nuade.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The player's own CIA-B timer is programmed with 1789773 / 125 = 14318 and runs N+1 CIA ticks per tick (~49.54 Hz).
#define OKT_TIMEBASE_HZ 709379
#define OKT_TICK_LENGTH 14319

typedef struct OktModule OktModule;

// Display
typedef struct OktPosition {
    bool valid; // false before the first tick
    int song_pos;
    int pattern;
    int row; // the line the last tick played
    int speed;
    int tick; // tick within the line
} OktPosition;

typedef struct OktVoiceState {
    int note;   // the last note started on the channel, 1-36 as in the pattern after clamping, 0 if none
    int sample; // the sample it plays
} OktVoiceState;

typedef struct OktPatternLine {
    uint8_t note;
    uint8_t sample_num;
    uint8_t effect;
    uint8_t effect_arg;
} OktPatternLine;

typedef struct OktPattern {
    int16_t line_num;
    OktPatternLine* lines; // [line_num * chan_num]
} OktPattern;

typedef struct OktSample {
    char name[21];
    uint32_t length;
    uint16_t repeat_start;
    uint16_t repeat_length;
    uint16_t volume;
    uint16_t mode; // 0 = 8-bit (7-bit), 1 = 4-channel, 2 = B (7-bit)
    int8_t* sample_data;
} OktSample;

typedef struct OktChannelInfo {
    uint8_t curr_note;
    uint8_t shown_note; // note (1-36) and sample of the last note the channel started, 0 if none
    uint8_t shown_sample;
    int16_t curr_period;
    uint32_t release_start;
    uint32_t release_length;
} OktChannelInfo;

// What the player has written to a Paula channel; handed to the voice at the end of each tick.
typedef struct OktChannel {
    int8_t* sample_data;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t volume; // 0-64 (Amiga range)
    uint32_t period;
    uint16_t panning; // 0=left, 1=right
    bool play_pending;
    bool release_pending;
} OktChannel;

struct OktModule {
    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    uint32_t samp_num;
    uint16_t patt_num;
    uint16_t song_length;
    uint16_t chan_num;
    uint16_t start_speed;

    bool channel_flags[4];
    uint8_t pattern_table[128];
    OktSample* samples;
    OktPattern* patterns;

    uint8_t chan_index[8];

    // Playing info
    uint16_t current_speed;
    uint16_t speed_counter;
    int16_t song_pos;
    int16_t new_song_pos;
    int16_t patt_pos;
    bool filter_status;

    OktPatternLine curr_line[8];
    OktChannelInfo chan_info[8];
    int8_t chan_vol[8];

    OktChannel channels[8];

    // Times the song ran back into a position it had already played
    uint32_t loop_count;
    bool end_reached;

    // Position visited tracking
    uint8_t visited[128];
};

// True when the data carries an Oktalyzer module mark.
bool okt_probe(const uint8_t* data, size_t size);

OktModule* okt_create(const uint8_t* data, size_t size);
void okt_destroy(OktModule* module);

int okt_subsong_count(const OktModule* module);
// (Re)start from the first position; the song plays through `voices`, which must outlive the module's use.
bool okt_start(OktModule* module, int subsong, NuadeVoices* voices);

int okt_channel_count(const OktModule* module);

// Run one player tick and set the voices. True when the song passed its loop point during it.
bool okt_tick(OktModule* module);

// Times the song ran back into a position it had already played.
uint32_t okt_loop_count(const OktModule* module);

OktPosition okt_position(const OktModule* module);
OktVoiceState okt_voice_state(const OktModule* module, int voice);
// The pattern at a song position, -1 outside the song.
int okt_order_pattern(const OktModule* module, int order);

const char* okt_instrument_name(const OktModule* module, int inst);

// Edit API
int okt_get_instrument_count(const OktModule* module);
int okt_get_num_patterns(const OktModule* module);
int okt_get_pattern_rows(const OktModule* module, int pattern);
int okt_get_num_positions(const OktModule* module);

void okt_get_cell(const OktModule* module, int pattern, int row, int channel, uint8_t* note, uint8_t* sample_num,
                  uint8_t* effect, uint8_t* effect_arg);
void okt_set_cell(OktModule* module, int pattern, int row, int channel, uint8_t note, uint8_t sample_num,
                  uint8_t effect, uint8_t effect_arg);

float okt_get_instrument_param(const OktModule* module, int inst, const char* param);
void okt_set_instrument_param(OktModule* module, int inst, const char* param, float value);

size_t okt_export(const OktModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
OktModule* okt_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
