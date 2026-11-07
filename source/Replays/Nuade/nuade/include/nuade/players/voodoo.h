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

// The player's tick rate: one vs_tick per VBlank.
#define VS_TICK_HZ 50

#define VS_PAULA_MAX_BYTES (0x40 * 2)
// Stack for gosub/loop
#define VS_STACK_MAX 64

typedef struct VsModule VsModule;

// Read-only state for displays.
typedef struct VsVoiceState {
    int note;       // period table index (transpose included) of the last note the voice started, -1 if none
    int instrument; // waveform number of the voice's current sample, -1 if none
} VsVoiceState;

// Track data (byte array)
typedef struct VsTrackData {
    uint8_t* track;
    int length;
} VsTrackData;

// Table (byte array, used for volume envelopes, period tables, waveform tables)
typedef struct VsTable {
    uint8_t* data;
    int length;
} VsTable;

// Waveform (32-byte synth waveform or raw sample)
typedef struct VsWaveform {
    int offset; // file offset, -1 for empty
    int8_t* data;
    int data_length;
    uint16_t sample_length; // only for Sample subclass (0 for plain waveform)
    bool is_sample;
} VsWaveform;

// Generic module data element (union-style via type tag)
typedef enum VsDataType { VS_DATA_NONE = 0, VS_DATA_TRACK, VS_DATA_TABLE, VS_DATA_WAVEFORM } VsDataType;

typedef struct VsModuleData {
    VsDataType type;
    union {
        VsTrackData track;
        VsTable table;
        VsWaveform waveform;
    } u;
} VsModuleData;

// Song info for one subsong
typedef struct VsSongInfo {
    int track_indices[4]; // indices into data[] for the 4 voice tracks
    int data_count;
    VsModuleData* data;
} VsSongInfo;

typedef struct VsStack {
    uint32_t items[VS_STACK_MAX];
    int top;
} VsStack;

// VoiceInfo
typedef struct VsVoiceInfo {
    int channel_number;

    // Waveform references (indices into song data)
    int sample1_idx; // -1 = empty
    uint32_t sample1_offset;
    int16_t sample1_number;
    int sample2_idx; // -1 = empty
    int16_t sample2_number;

    int8_t audio_buffer[2][VS_PAULA_MAX_BYTES]; // bytes past VS_WAVE_BYTES stay zero
    uint8_t use_audio_buffer;

    VsStack stack;

    int track_idx; // index into song data for current track
    int track_position;
    uint8_t tick_counter;

    bool new_note;
    int16_t last_note;       // period table index of the last note started, -1 if none
    int16_t last_instrument; // sample number selected when that note started, -1 if none
    int8_t transpose;
    uint16_t note_period;
    uint16_t target_period;
    uint8_t current_volume;
    uint8_t final_volume;
    uint8_t master_volume;
    uint8_t reset_flags;

    uint8_t portamento_tick_counter;
    uint8_t portamento_increment;
    bool portamento_direction;
    uint8_t portamento_delay;
    uint8_t portamento_duration;

    int vol_env_idx; // index into song data for vol envelope, -1 = none
    uint8_t vol_env_position;
    uint8_t vol_env_tick_counter;
    uint8_t vol_env_delta;

    int period_table_idx; // index into song data, -1 = none
    uint8_t period_table_position;
    uint8_t period_table_tick_counter;
    uint8_t period_table_command;

    int waveform_table_idx; // index into song data, -1 = none
    uint8_t waveform_table_position;

    uint8_t waveform_start_position;
    uint8_t waveform_position;
    uint8_t waveform_tick_counter;
    uint8_t waveform_increment;
    uint8_t waveform_mask;
    uint8_t synthesis_mode;
    uint8_t morph_speed;
} VsVoiceInfo;

// Paula registers of one channel. The player writes them during a tick and hands them to the voice at its end.
typedef struct VsPaulaChannel {
    uint16_t period;
    uint16_t volume;
    uint16_t paula_length; // Length in words

    // Address latch (AUDxLC), taken by the audio interrupt
    const int8_t* pending_sample;
    uint32_t pending_size; // bytes of data behind pending_sample
    uint32_t pending_offset;

    bool latched; // the audio interrupt ran: the voice loops the latched address
    NuadeSample loop;
} VsPaulaChannel;

struct VsModule {
    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    // Module data
    int num_subsongs;
    VsSongInfo* subsongs;

    // All waveforms for sample numbering
    int num_all_samples;
    int* all_sample_indices; // indices into current song data

    // Current song
    VsSongInfo* current_song;

    // Voices
    VsVoiceInfo voices[4];
    VsPaulaChannel channels[4];

    uint16_t all_voices_taken;
    uint32_t loop_count;
    int8_t empty_sample[256];

    NuadeVoices* output;
};

// True when the data carries a Voodoo Supreme footer mark.
bool vs_probe(const uint8_t* data, size_t size);

VsModule* vs_create(const uint8_t* data, size_t size);
void vs_destroy(VsModule* module);

int vs_subsong_count(const VsModule* module);
// (Re)start a subsong from its beginning, playing through `voices`.
bool vs_start(VsModule* module, int subsong, NuadeVoices* voices);

// Run one tick and hand the result to the voices. True when the song looped during it.
bool vs_tick(VsModule* module);

VsVoiceState vs_voice_state(const VsModule* module, int channel);

// Edit API
int vs_get_instrument_count(const VsModule* module);
float vs_get_instrument_param(const VsModule* module, int inst, const char* param);
void vs_set_instrument_param(VsModule* module, int inst, const char* param, float value);
size_t vs_export(const VsModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
VsModule* vs_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
