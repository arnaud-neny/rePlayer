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

#define RK_CHANNELS 4

// Tick length in RK_TIMEBASE_HZ (CIA clock) units.
#define RK_TIMEBASE_HZ 709379

// Every note lasts a multiple of 4 ticks (the wait byte counts in groups of 4).
#define RK_TICKS_PER_ROW 4

typedef struct RkModule RkModule;

// The note a channel started last: its period and instrument number (period 0, instrument -1 before the first).
typedef struct RkVoiceState {
    uint16_t period;
    int16_t instrument;
} RkVoiceState;

typedef enum RkInstrumentType { RK_INSTRUMENT_SAMPLE = 0, RK_INSTRUMENT_SYNTHESIS = 1 } RkInstrumentType;

typedef struct RkAdsrPoint {
    uint8_t point;
    uint8_t increment;
} RkAdsrPoint;

typedef struct RkInstrument {
    int sample_number;
    int vibrato_number;
    RkInstrumentType type;
    uint8_t phase_speed;
    uint8_t phase_length_in_words;
    uint8_t vibrato_speed;
    uint8_t vibrato_depth;
    uint8_t vibrato_delay;
    RkAdsrPoint adsr[4];
    int8_t phase_value;
    bool phase_direction;
    uint8_t phase_position;
} RkInstrument;

typedef struct RkSample {
    int sample_number;
    uint16_t length_in_words;
    uint16_t phase_index;
} RkSample;

typedef struct RkTrack {
    int track_number;
    int16_t transpose;
    uint16_t number_of_repeat_times;
} RkTrack;

typedef struct RkPositionList {
    RkTrack* tracks;
    int track_count;
} RkPositionList;

typedef struct RkSongInfo {
    RkPositionList positions[4];
} RkSongInfo;

typedef struct RkVoiceInfo {
    int channel_number;

    RkPositionList* position_list;
    int track_list_position;
    uint8_t* track_data;
    int track_data_position;
    uint16_t track_repeat_counter;
    uint8_t wait_counter;

    RkInstrument* instrument;
    int16_t instrument_number;

    int8_t* arpeggio_values;
    uint16_t arpeggio_position;

    uint8_t current_note;
    uint16_t started_period;    // period of the note last started, 0 if none
    int16_t started_instrument; // instrument of the note last started, -1 if none
    int16_t transpose;
    uint16_t period;

    uint16_t portamento_end_period;
    uint8_t portamento_increment;

    uint8_t vibrato_delay;
    uint16_t vibrato_position;

    uint16_t adsr_state;
    uint8_t adsr_speed;
    int8_t adsr_speed_counter;
    uint8_t volume;

    uint8_t phase_speed_counter;

    // Hardware register staging
    bool set_hardware;
    int16_t sample_number;
    int8_t* sample_data;
    uint32_t sample_length;
    bool set_loop;
} RkVoiceInfo;

// Paula registers: written during a tick, handed to the voices at its end
typedef struct RkChannel {
    bool active;       // DMA on
    bool play_pending; // a new sample starts at the end of this tick
    int8_t* sample_data;
    uint32_t sample_length;
    uint32_t loop_length; // 0 = one-shot, else the whole sample loops
    uint16_t period;
    uint16_t volume; // 0-64
} RkChannel;

struct RkModule {
    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    uint32_t loop_count;
    bool end_reached[4]; // per-channel end tracking

    bool clear_adsr_state_on_portamento;
    uint16_t cia_value;
    int current_song;

    RkSongInfo* sub_songs;
    int num_sub_songs;

    uint8_t** tracks;
    int num_tracks;
    // File offset and byte length of each loaded track, so an edit addressed
    // by file offset (the grid's cell -> stream map lives in the importer)
    // lands in the track the replayer reads.
    int* track_offsets;
    int* track_lengths;

    int8_t** arpeggios;
    int num_arpeggios;

    int8_t** vibratos;
    int* vibrato_lengths; // bytes in each vibrato table
    int num_vibratos;

    RkInstrument* instruments;
    int num_instruments;

    RkSample* samples;
    int num_samples;

    int8_t** sample_data;
    int num_sample_data;

    // Playing state
    uint8_t global_volume;
    bool setup_new_sub_song;
    uint16_t sub_song_number;

    RkVoiceInfo voices[4];

    RkChannel channels[4];
    NuadeVoices* nuade_voices;

    // Static arpeggio table for default
    int8_t default_arpeggio[12];
};

// True when the data carries the Ron Klaren sound module mark.
bool rk_probe(const uint8_t* data, size_t size);

RkModule* rk_create(const uint8_t* data, size_t size);
void rk_destroy(RkModule* module);

int rk_subsong_count(const RkModule* module);
// (Re)start a subsong from its first position; the tick drives `voices`, which must outlive the playback.
bool rk_start(RkModule* module, int subsong, NuadeVoices* voices);

int rk_channel_count(const RkModule* module);
uint32_t rk_tick_length(const RkModule* module);

// Run one player tick and hand the result to the voices. True when the song wrapped around during it.
bool rk_tick(RkModule* module);

// Times the song wrapped around (every channel passed its position list end, or an end-song command).
uint32_t rk_loop_count(const RkModule* module);
RkVoiceState rk_voice_state(const RkModule* module, int channel);

// Edit API
int rk_get_instrument_count(const RkModule* module);

float rk_get_instrument_param(const RkModule* module, int inst, const char* param);
void rk_set_instrument_param(RkModule* module, int inst, const char* param, float value);

size_t rk_export(const RkModule* module, uint8_t* out, size_t max_size);

// Set the note of the note command at file_offset, as the grid shows it:
// `note` is the period-table index heard at sub-song 1's position-list entry
// `position` on `channel` (that entry's transpose included). The command's
// wait byte (the row's duration) is kept. Returns 1 when written, 0 when no
// note command of that entry's track starts there. The importer maps a grid
// cell to the offset.
int rk_set_cell(RkModule* module, int position, int channel, uint32_t file_offset, uint8_t note);

// The note command at file_offset as (note << 8) | wait, or -1.
int rk_get_cell(const RkModule* module, uint32_t file_offset);

// The player's state behind a song it opened; 0 when the song is another player's.
RkModule* rk_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
