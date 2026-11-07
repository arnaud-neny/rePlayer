// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
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

#define BD_VOICE_COUNT 4

typedef struct BdModule BdModule;

typedef struct BdFeatures {
    int master_volume_fade_version; // -1 = None
    bool set_dma_in_sample_handlers;
    bool enable_counter;
    bool enable_sample_effects;
    bool enable_final_volume_slide;
    bool enable_volume_fade;
    bool enable_portamento;
    bool check_for_ticks;
    bool extra_tick_arg;
    bool uses_9x_track_effects;
    bool uses_cx_track_effects;

    uint8_t max_track_value;
    bool enable_c0_track_loop;
    bool enable_f0_track_loop;

    uint8_t max_sample_mapping_value;
    int get_sample_mapping_version;
    int set_sample_mapping_version;
} BdFeatures;

typedef struct BdSample {
    int8_t* sample_data;
    uint16_t length;
    uint32_t loop_offset;
    uint16_t loop_length;
    // What Paula fetches when the loop length register is written as 0: 65536 words from the start of the sample
    // info table (the module bytes, up to the end of the file)
    const int8_t* wrap_data;
    uint32_t wrap_length;

    uint16_t volume;
    int16_t volume_fade_speed;

    int16_t portamento_duration;
    int16_t portamento_add_value;

    uint16_t vibrato_depth;
    uint16_t vibrato_add_value;

    int16_t note_transpose;
    uint16_t fine_tune_period;
} BdSample;

typedef struct BdSongInfo {
    uint16_t position_lists[4];
} BdSongInfo;

typedef struct BdPositionList {
    uint16_t offset_key;
    uint8_t* data;
    size_t length;
} BdPositionList;

typedef enum BdSampleHandler {
    BdSampleHandler_None = 0,
    BdSampleHandler_PlayOnce,
    BdSampleHandler_Loop,
    BdSampleHandler_VolumeFade,
} BdSampleHandler;

typedef struct BdVoiceInfo {
    bool channel_enabled;

    uint8_t* position_list;
    size_t position_list_length;
    int current_position;
    int next_position;

    int playing_track;
    uint8_t* track;
    size_t track_length;
    int next_track_position;

    bool switch_to_next_position;
    uint8_t track_loop_counter;
    uint8_t ticks_left_for_next_track_command;

    int8_t transpose;
    uint8_t transposed_note;
    uint8_t previous_transposed_note;
    bool use_new_note;

    uint8_t portamento1_enabled;
    bool portamento2_enabled;
    uint8_t portamento_start_delay;
    uint8_t portamento_duration;
    int8_t portamento_delta_note_number;

    uint8_t portamento_control_flag;
    uint8_t portamento_start_delay_counter;
    uint8_t portamento_duration_counter;
    int portamento_add_value;

    bool volume_fade_enabled;
    uint8_t volume_fade_init_speed;
    uint8_t volume_fade_duration;
    int16_t volume_fade_init_add_value;

    bool volume_fade_running;
    uint8_t volume_fade_speed;
    uint8_t volume_fade_speed_counter;
    uint8_t volume_fade_duration_counter;
    int16_t volume_fade_add_value;
    int16_t volume_fade_value;

    uint16_t channel_volume;
    uint16_t channel_volume_slide_speed;
    int16_t channel_volume_slide_add_value;

    BdSample* sample_info;
    BdSample* sample_info2;

    uint8_t sample_mapping[10];
} BdVoiceInfo;

typedef struct BdVoicePlaybackInfo {
    BdSample* playing_sample;
    uint8_t sample_play_ticks_counter;

    uint16_t note_period;
    int16_t final_volume;

    uint16_t final_volume_slide_speed;
    uint16_t final_volume_slide_speed_counter;
    int16_t final_volume_slide_add_value;

    uint16_t loop_delay_counter;

    // For the display: the note and sample parsed and not yet started (-1 if none), and the ones of the latest start.
    int32_t pending_note;
    int32_t pending_instrument;
    int32_t started_note;
    int32_t started_instrument;

    int16_t portamento_add_value;

    int16_t sample_portamento_duration;
    int16_t sample_portamento_add_value;

    uint16_t sample_vibrato_depth;
    int16_t sample_vibrato_add_value;

    int16_t sample_period_add_value;

    BdSampleHandler handler;

    // Amiga hardware registers
    bool dma_enabled;
    int8_t* sample_data;
    uint16_t sample_length;
    double pitch_hz;

    // The voice this channel plays on; it keeps playing until stopped or until a one-shot runs off its end
    NuadeVoices* mixer;
    uint32_t voice;
    bool active;
} BdVoicePlaybackInfo;

typedef struct BdGlobalPlayingInfo {
    bool enable_playing;

    int16_t master_volume;
    int16_t master_volume_fade_speed;
    int16_t master_volume_fade_speed_counter;

    uint16_t counter;
} BdGlobalPlayingInfo;

typedef struct BdPlayer {
    BdGlobalPlayingInfo playing_info;
    BdVoiceInfo voices[BD_VOICE_COUNT];
    BdVoicePlaybackInfo playback[BD_VOICE_COUNT];

    NuadeVoices* mixer;
    int current_subsong;
} BdPlayer;

struct BdModule {
    // Offsets found during identification
    int sub_song_list_offset;
    int track_offset_table_offset;
    int tracks_offset;
    int sample_info_offset_table_offset;

    BdFeatures features;

    BdSongInfo* sub_songs;
    size_t sub_song_count;

    BdPositionList* position_lists;
    size_t position_list_count;

    uint8_t** tracks;
    size_t* track_lengths;
    size_t track_count;

    BdSample* samples;
    size_t sample_count;

    // Raw file data (kept for sample data references)
    uint8_t* file_data;
    size_t file_size;

    BdPlayer* player;
};

// True when the data is a Ben Daglish module (the player code at the start of the file is checked).
bool bd_probe(const uint8_t* data, size_t size);

BdModule* bd_create(const uint8_t* data, size_t size);
void bd_destroy(BdModule* module);

int bd_subsong_count(const BdModule* module);
int bd_channel_count(const BdModule* module);

// (Re)start a subsong from its beginning; the player plays on `voices` (Paula channel numbering).
bool bd_start(BdModule* module, int subsong, NuadeVoices* voices);

// One VBlank tick. Returns true when the song restarted from its beginning on this tick.
bool bd_tick(BdModule* module);

// The note and sample index of the latest start on a voice; false before the first one.
bool bd_voice_started(const BdModule* module, uint32_t voice, int32_t* note, int32_t* instrument);

// The player's state behind a song it opened; 0 when the song is another player's.
BdModule* bd_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
