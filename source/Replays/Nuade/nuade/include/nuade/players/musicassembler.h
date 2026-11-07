// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
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

/* Player ticks per second (VBlank). */
#define MA_TICK_HZ 50

typedef struct MaModule MaModule;

typedef struct MaTickResult {
    bool looped; /* every voice wrapped to the start of its position list during this tick */
    bool ended;  /* the song data ran out or is malformed: nothing more can be played */
} MaTickResult;

/* True when the data has the Music Assembler player header (a 68k module). */

typedef struct MaSongInfo {
    uint8_t start_speed;
    uint16_t position_lists[4];
} MaSongInfo;

typedef struct MaPositionInfo {
    uint8_t track_number;
    uint8_t transpose;
    int8_t repeat_counter;
} MaPositionInfo;

typedef struct MaTrack {
    uint8_t* data;
    size_t length;
} MaTrack;

typedef struct MaInstrument {
    uint8_t sample_number;
    uint8_t attack;
    uint8_t decay_sustain;
    uint8_t release;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint8_t vibrato_level;
    uint8_t arpeggio;
    uint8_t fx_arp_spdlp;
    uint8_t hold;
    uint8_t key_wave_rate;
    uint8_t wave_level_speed;
} MaInstrument;

typedef struct MaSample {
    char name[17];
    const int8_t* sample_data;
    uint32_t data_bytes; // bytes readable at sample_data
    uint16_t length;
    uint16_t loop_length;
} MaSample;

typedef struct MaVoiceInfo {
    int channel_number;

    const MaPositionInfo* position_list;
    uint16_t position_list_length;

    uint16_t current_position;
    uint16_t current_track_row;
    int8_t track_repeat_counter;
    int8_t row_delay_counter;

    uint8_t flag;

    const MaInstrument* current_instrument;

    uint8_t current_note;
    uint8_t current_sample_number;
    uint8_t transpose;

    uint8_t volume;
    uint8_t sustain_counter;
    uint8_t arpeggio_counter;
    uint8_t arpeggio_value_to_use;
    uint8_t portamento_or_vibrato_value;
    uint8_t vibrato_delay_counter;
    uint8_t vibrato_direction;
    uint8_t vibrato_speed_counter;
    uint8_t wave_speed_counter;

    int16_t portamento_add_value;
    int16_t vibrato_add_value;

    uint16_t wave_length_modifier;

    bool decrease_volume;
    bool wave_direction;

    const int8_t* sample_data;
    uint32_t sample_data_bytes;
    uint32_t sample_start_offset;
    uint16_t sample_length;

    // The sample the next tick end hands to the voice: it starts at once if the voice is idle, else it is queued
    const int8_t* pending_data;
    uint32_t pending_data_bytes;
    uint32_t pending_start_offset;
    uint16_t pending_length_words;
    bool pending_loops; // the whole sample repeats
    bool has_pending_sample;
    bool active;             // the voice plays (read back from the mixer at the start of a tick)
    bool active_before_tick; // `active` as the tick started; a retrigger clears `active` during the tick
    uint16_t current_period;
    uint16_t current_volume;
    int32_t started_note;       // current_note when a sample last started or queued, -1 if none
    int32_t started_instrument; // instrument index then, -1 if none
} MaVoiceInfo;

typedef struct MaGlobalPlayingInfo {
    uint16_t master_volume;
    uint8_t speed;
    uint8_t speed_counter;
} MaGlobalPlayingInfo;

typedef struct MaPositionListEntry {
    uint16_t offset;
    MaPositionInfo* positions;
    uint16_t length;
} MaPositionListEntry;

typedef struct MaPlayer {
    char error_message[160];

    const MaModule* module;
    NuadeVoices* mixer;

    int current_subsong;
    bool ended; // set during a tick when the song data ran out or is malformed
    bool channel_end[4];

    // The song loops when every voice has wrapped to the start of its position list since the last loop.
    uint32_t loop_count;
    uint8_t wrapped_mask;

    MaGlobalPlayingInfo playing_info;
    MaVoiceInfo voices[4];

} MaPlayer;

struct MaModule {
    char error_message[160];

    int sub_song_count;
    int sub_song_speed_offset;
    int sub_song_position_list_offset;
    int module_start_offset;
    int instrument_info_offset_offset;
    int sample_info_offset_offset;
    int tracks_offset_offset;

    MaSongInfo* songs;
    size_t song_count;

    MaPositionListEntry* position_lists;
    size_t position_list_count;

    MaTrack* tracks;
    size_t track_count;

    MaInstrument* instruments;
    size_t instrument_count;

    MaSample* samples;
    size_t sample_count;

    MaPlayer* player;
};

bool ma_probe(const uint8_t* data, size_t size);

MaModule* ma_create(const uint8_t* data, size_t size);
void ma_destroy(MaModule* module);

int ma_subsong_count(const MaModule* module);
/* (Re)start a subsong from its first position; the player drives `voices` (Paula channel n = voice n). */
bool ma_start(MaModule* module, int subsong, NuadeVoices* voices);

/* Run one player tick and hand the result to the voices. */
MaTickResult ma_tick(MaModule* module);

/* Ticks per row of a subsong (the speed it starts with; the song never changes it), 0 when out of range. */
int ma_start_speed(const MaModule* module, int subsong);
/* Note (track note plus transpose) and instrument index of the last sample start on a voice; false before the first. */
bool ma_voice_started(const MaModule* module, int voice, int* note, int* instrument);

int ma_sample_count(const MaModule* module);
/* Sample name, "" when out of range. */
const char* ma_sample_name(const MaModule* module, int index);

/* Track data access for pattern editing */
size_t ma_track_count(const MaModule* module);
const uint8_t* ma_track_data(const MaModule* module, size_t trackIdx, size_t* out_length);
void ma_track_replace_data(MaModule* module, size_t trackIdx, uint8_t* newdata, size_t newlen);

int ma_instrument_count(const MaModule* module);

/* Instrument parameter get/set by name */
float ma_instrument_get_param(const MaModule* module, int index, const char* param);
void ma_instrument_set_param(MaModule* module, int index, const char* param, float value);

// The player's state behind a song it opened; 0 when the song is another player's.
MaModule* ma_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
