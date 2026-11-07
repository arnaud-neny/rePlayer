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

// The player's tick rate: one player tick per VBlank.
#define DW_TICK_HZ 50

// Track storage: we store tracks as a list of (offset, data, length) entries
#define DW_MAX_TRACKS 4096

typedef struct DwModule DwModule;

typedef struct DwSample {
    int16_t sample_number;
    int8_t* sample_data;
    uint32_t length;
    uint32_t data_size; // bytes allocated for sample_data; length never exceeds it
    int32_t loop_start; // -1 = no loop
    uint16_t volume;
    uint16_t fine_tune_period;
    int8_t transpose;
} DwSample;

typedef struct DwPositionList {
    uint32_t* track_offsets;
    int track_offset_count;
    uint16_t restart_position;
} DwPositionList;

typedef struct DwSongInfo {
    uint16_t speed;
    uint8_t delay_counter_speed;
    DwPositionList* position_lists; // [number_of_channels]
} DwSongInfo;

typedef struct DwChannelInfo {
    int channel_number;

    uint32_t* position_list;
    int position_list_length;
    uint16_t current_position;
    uint16_t restart_position;

    const uint8_t* track_data;
    int track_data_length;
    int track_data_position;

    DwSample* current_sample_info;
    uint8_t note;
    int8_t transpose;
    int started_note;       // note of the last sample start, -1 before the first
    int started_instrument; // sample number of the last sample start, -1 before the first

    bool enable_half_volume;

    uint16_t speed;
    uint16_t speed_counter;

    uint8_t* arpeggio_list;
    int arpeggio_list_length;
    int arpeggio_list_position;

    uint8_t* envelope_list;
    int envelope_list_length;
    int envelope_list_position;
    uint8_t envelope_speed;
    int8_t envelope_counter;

    bool slide_enabled;
    int8_t slide_speed;
    uint8_t slide_counter;
    int16_t slide_value;

    int8_t vibrato_direction;
    uint8_t vibrato_speed;
    uint8_t vibrato_value;
    uint8_t vibrato_max_value;
} DwChannelInfo;

typedef struct DwGlobalPlayingInfo {
    int8_t transpose;

    uint16_t volume_fade_speed;

    uint16_t global_volume;
    uint8_t global_volume_fade_speed;
    uint8_t global_volume_fade_counter;

    uint16_t square_change_position;
    bool square_change_direction;

    uint8_t extra_counter;

    uint8_t delay_counter_speed;
    uint16_t delay_counter;

    uint16_t speed;
} DwGlobalPlayingInfo;

typedef struct DwTrackEntry {
    uint32_t offset;
    uint8_t* data;
    int length;
} DwTrackEntry;

// What a Paula channel is given: written during a tick, handed to the voice at its end
typedef struct DwPaulaChannel {
    bool active;       // sample is playing (a one-shot that ran off its end clears it at the next tick)
    bool play_pending; // a new sample was started this tick

    int8_t* sample_data;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length; // 0 = one-shot

    uint16_t volume; // 0-64 Amiga volume
    uint32_t period; // Amiga period
} DwPaulaChannel;

struct DwModule {
    // Module flags (from identification)
    bool old_player;
    bool uses_32bit_pointers;

    int start_offset;

    int sample_info_offset;
    int sample_data_offset;
    int sub_song_list_offset;
    int arpeggio_list_offset;
    int envelope_list_offset;
    int channel_volume_offset;

    int number_of_samples;
    int number_of_channels;

    // Loaded data
    DwSongInfo* song_info_list;
    int song_info_count;

    DwTrackEntry tracks[DW_MAX_TRACKS];
    int track_count;

    uint8_t** arpeggios;
    int* arpeggio_lengths;
    int arpeggio_count;

    uint8_t** envelopes;
    int* envelope_lengths;
    int envelope_count;

    DwSample* samples;
    uint16_t* channel_volumes;

    // Feature flags (from play function analysis)
    bool enable_sample_transpose;
    bool enable_channel_transpose;
    bool enable_global_transpose;

    uint8_t new_sample_cmd;
    uint8_t new_envelope_cmd;
    uint8_t new_arpeggio_cmd;

    bool enable_arpeggio;
    bool enable_envelopes;
    bool enable_vibrato;

    bool enable_volume_fade;
    bool enable_half_volume;
    bool enable_global_volume_fade;
    bool enable_set_global_volume;

    bool enable_square_waveform;
    int square_waveform_sample_number;
    uint32_t square_waveform_sample_length;
    uint16_t square_change_min_position;
    uint16_t square_change_max_position;
    uint8_t square_change_speed;
    int8_t square_byte1;
    int8_t square_byte2;

    bool use_extra_counter;

    bool enable_delay_counter;
    bool enable_delay_multiply;
    bool enable_delay_speed;

    const uint16_t* periods;
    int periods_count;

    // Playing state
    DwGlobalPlayingInfo playing_info;
    DwChannelInfo* channels;
    DwPaulaChannel* paula;
    NuadeVoices* voices;

    int current_song;
    bool ended;                 // the song asked to stop
    uint32_t loop_count;        // times every channel has wrapped its position list
    uint32_t end_reached_mask;  // channels that reached the end of their position list // rePlayer

    // Copy of raw module data for loading
    uint8_t* raw_data;
    size_t raw_data_size;
};

// True when the data passes the player's identification (it scans the module's 68k player code).
bool dw_probe(const uint8_t* data, size_t size);

DwModule* dw_create(const uint8_t* data, size_t size);
void dw_destroy(DwModule* module);

int dw_subsong_count(const DwModule* module);
// (Re)start a subsong from its beginning; the player drives `voices` from then on.
bool dw_start(DwModule* module, int subsong, NuadeVoices* voices);

// Voices that are mixed: the module's channels, at most four.
int dw_voice_count(const DwModule* module);

// Run one player tick (one VBlank) and set the voices.
void dw_tick(DwModule* module);

// Times every channel has wrapped back to its restart position.
uint32_t dw_loop_count(const DwModule* module);
// The song asked to stop.
bool dw_ended(const DwModule* module);

// The song speed of a subsong: ticks per step of the shortest note length, 0 if the subsong does not exist.
uint32_t dw_subsong_speed(const DwModule* module, int subsong);
// The note and sample number of the last sample start on a voice (-1 before the first). False for a bad voice.
bool dw_voice_started(const DwModule* module, int voice, int* note, int* instrument);

// The player's state behind a song it opened; 0 when the song is another player's.
DwModule* dw_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
