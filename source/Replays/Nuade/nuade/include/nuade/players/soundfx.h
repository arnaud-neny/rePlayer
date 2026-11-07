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

#define SFX_CHANNELS 4
#define SFX_INSTRUMENTS 31

// The CIA clock: the timebase of sfx_tick_length.
#define SFX_TICK_HZ 709379

#define SFX_PATTERN_ROWS 64
#define SFX_TICKS_PER_ROW 6

// Commands that sit in the period field of a pattern word.
#define SFX_SPECIAL_STOP 0xfffe    // stop the channel
#define SFX_SPECIAL_PIC 0xfffd     // leave the channel as it is
#define SFX_SPECIAL_BREAK 0xfffc   // end the pattern after this row (2.0 only)
#define SFX_SPECIAL_UNKNOWN 0xfffb // does nothing

#define SFX_MAX_VISITED 16

typedef struct SfxModule SfxModule;

typedef struct SfxPosition {
    uint32_t position; // index in the order list
    uint32_t row;
    uint32_t speed; // ticks per row
} SfxPosition;

typedef struct SfxVoiceState {
    uint16_t period; // of the last note played on the channel, 0 if none
    uint8_t sample;  // its sample number (1..31), 0 if none
} SfxVoiceState;

typedef struct SfxCell {
    uint16_t period;
    uint8_t sample;
    uint8_t effect;
    uint8_t effect_arg;
    uint16_t special; // one of SFX_SPECIAL_*, 0 if the period field is a note
} SfxCell;

struct SfxLayout;

typedef struct SfxSample {
    char name[23];
    int8_t* sample_addr;
    uint32_t length;
    uint16_t volume;
    uint32_t loop_start;
    uint32_t loop_length;
} SfxSample;

// What the tick asks of the voice: restart the registers' sample, or stop it.
typedef enum SfxVoiceAction {
    SfxVoiceAction_None,
    SfxVoiceAction_Play,
    SfxVoiceAction_Stop,
} SfxVoiceAction;

typedef struct SfxChannel {
    uint32_t pattern_data;
    int8_t* sample;
    uint32_t sample_len;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t current_note;
    uint8_t sample_number;   // 1..31 of the last sample selected, 0 if none
    uint16_t started_period; // period and sample number of the last note played on the channel
    uint8_t started_sample;
    uint16_t volume;
    int16_t step_value;
    uint16_t step_note;
    uint16_t step_end_note;
    uint16_t slide_control;
    bool slide_direction;
    uint16_t slide_param;
    uint16_t slide_period;
    uint16_t slide_speed;

    // Paula registers, handed to the voice at the end of the tick
    int8_t* ch_sample_data;
    uint32_t ch_sample_length;
    uint32_t ch_loop_start;
    uint32_t ch_loop_length;
    uint16_t ch_volume;
    uint16_t ch_period;
    SfxVoiceAction voice_action;
} SfxChannel;

typedef struct SfxGlobalPlayingInfo {
    uint16_t timer;
    uint32_t track_pos;
    uint32_t pos_counter;
    bool break_flag;
    uint32_t shown_track_pos; // the position and row sfx_play_sound played last
    uint32_t shown_row;
} SfxGlobalPlayingInfo;

// Header layout of the two module generations. The sample size table comes first and ends where the mark starts.
typedef struct SfxLayout {
    uint32_t sample_count;
    uint32_t mark_offset;
    const char* mark;
    uint32_t extra_pattern_skip; // bytes between the order table and the first pattern
    const char* format_name;
    bool has_break; // 0xfffc ends the pattern; 1.x plays it as a note with that period
} SfxLayout;

struct SfxModule {
    const struct SfxLayout* layout;
    bool has_break; // from the layout
    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    SfxSample samples[SFX_INSTRUMENTS];

    uint8_t orders[128];
    uint32_t song_length;
    uint16_t delay;
    uint16_t max_pattern;

    uint32_t** patterns; // max_pattern arrays, each 4*64 uint32s

    SfxGlobalPlayingInfo playing_info;
    SfxChannel channels[SFX_CHANNELS];

    bool end_reached;
    uint32_t loop_count;

    // Visited positions for end detection
    uint32_t visited[SFX_MAX_VISITED];
};

// True when the data carries the SoundFX 2.0 module mark.
bool sfx_probe(const uint8_t* data, size_t size);

SfxModule* sfx_create(const uint8_t* data, size_t size);
void sfx_destroy(SfxModule* module);

int sfx_subsong_count(const SfxModule* module);
// (Re)start from the first position; the module plays on `voices` (voice n is Paula channel n).
bool sfx_start(SfxModule* module, int subsong, NuadeVoices* voices);

int sfx_channel_count(const SfxModule* module);
// Length of one player tick in 1 / SFX_TICK_HZ seconds.
uint32_t sfx_tick_length(const SfxModule* module);
// Run one player tick and hand the result to the voices. True when the song looped during it.
bool sfx_tick(SfxModule* module);

// Times the song ran back into a position it had already played.
uint32_t sfx_loop_count(const SfxModule* module);

const char* sfx_format_name(const SfxModule* module);
// Empty when the sample has no name.
const char* sfx_instrument_name(const SfxModule* module, int inst);

// The row whose notes were started by the last tick.
SfxPosition sfx_position(const SfxModule* module);
// Entries of the order list.
uint32_t sfx_position_count(const SfxModule* module);

SfxVoiceState sfx_voice_state(const SfxModule* module, int channel);

// `position` 0 .. sfx_position_count - 1, `channel` 0..3, `row` 0..63.
SfxCell sfx_pattern_cell(const SfxModule* module, int position, int channel, int row);
// Rows the pattern at `position` plays before the next one.
uint32_t sfx_pattern_rows(const SfxModule* module, int position);

// Edit API
int sfx_get_instrument_count(const SfxModule* module);
int sfx_get_num_patterns(const SfxModule* module);
void sfx_get_cell(const SfxModule* module, int pattern, int row, int channel, uint16_t* period, uint8_t* sample,
                  uint8_t* effect, uint8_t* effect_arg);
void sfx_set_cell(SfxModule* module, int pattern, int row, int channel, uint16_t period, uint8_t sample, uint8_t effect,
                  uint8_t effect_arg);
float sfx_get_instrument_param(const SfxModule* module, int inst, const char* param);
void sfx_set_instrument_param(SfxModule* module, int inst, const char* param, float value);
size_t sfx_export(const SfxModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
SfxModule* sfx_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
