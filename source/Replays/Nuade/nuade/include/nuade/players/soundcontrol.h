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

#define SC_CHANNELS 4
// The player's tick rate: one tick per VBlank.
#define SC_TICK_HZ 50

#define SC_REPEAT_LIST_MAX 71
#define SC_REPEAT_STACK_MAX 32

typedef struct ScModule ScModule;

// Read-only state for the display hooks.
typedef struct ScVoiceState {
    int32_t note;       // linear note number (12 per octave) last started on the channel, -1 if none
    int32_t instrument; // its sample or instrument number, -1 if none
} ScVoiceState;

typedef enum ScModuleType { SC_TYPE_UNKNOWN = 0, SC_TYPE_3X, SC_TYPE_40, SC_TYPE_50 } ScModuleType;

typedef enum ScEnvelopeCommand {
    SC_ENV_ATTACK = 0,
    SC_ENV_DECAY,
    SC_ENV_SUSTAIN,
    SC_ENV_RELEASE,
    SC_ENV_DONE
} ScEnvelopeCommand;

typedef enum ScPlaySampleCommand { SC_PLAY_MUTE = 0, SC_PLAY_PLAY } ScPlaySampleCommand;

typedef struct ScPosition {
    uint8_t track_number;
} ScPosition;

typedef struct ScPositionList {
    ScPosition* positions[6]; // 6 channels of position lists
    int position_count;
} ScPositionList;

typedef struct ScEnvelope {
    uint8_t attack_speed;
    uint8_t attack_increment;
    uint8_t decay_speed;
    uint8_t decay_decrement;
    uint16_t decay_value;
    uint8_t release_speed;
    uint8_t release_decrement;
} ScEnvelope;

typedef struct ScSampleCommandInfo {
    uint16_t command;
    uint16_t argument1;
    uint16_t argument2;
} ScSampleCommandInfo;

typedef struct ScInstrument {
    ScEnvelope envelope;
    ScSampleCommandInfo* sample_commands;
    int sample_command_count;
} ScInstrument;

typedef struct ScSample {
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_end;
    int16_t note_transpose;
    int8_t* sample_data;
    int sample_data_length;
} ScSample;

typedef struct ScModuleData {
    uint16_t speed;

    ScPositionList position_list;

    uint8_t** tracks; // up to 256 tracks
    int* track_lengths;
    int num_tracks_allocated; // always 256

    ScInstrument* instruments;
    int num_instruments;

    ScSample* samples;
    int num_samples;
} ScModuleData;

typedef struct ScSongInfo3x {
    uint16_t start_position;
    uint16_t end_position;
} ScSongInfo3x;

typedef struct ScVoiceInfo3x {
    uint16_t wait_counter;
    uint8_t* track;
    int track_position;
    int8_t transpose; // only 3.2
} ScVoiceInfo3x;

typedef struct ScGlobalInfo3x {
    uint16_t song_position;
    uint16_t speed_counter;
    uint16_t speed_counter2;
} ScGlobalInfo3x;

typedef struct ScSongInfo40_50 {
    uint16_t start_position;
    uint16_t end_position;
    uint16_t speed;
    uint16_t restart_position; // where the song continues after its end position
} ScSongInfo40_50;

typedef struct ScVoiceInfo40_50 {
    uint16_t wait_counter;
    uint8_t* track;
    int track_position;

    int16_t transpose;
    uint16_t transposed_note;
    uint16_t sample_transposed_note;
    uint16_t period;

    uint16_t instrument_number;
    uint16_t sample_command_wait_counter;
    ScSampleCommandInfo* sample_command_list;
    int sample_command_list_count;
    int sample_command_position;
    ScPlaySampleCommand play_sample_command;

    uint16_t sample_number;
    int32_t note; // linear note number of the track note, as set up
    ScSample* sample;
    uint16_t sample_length;
    int8_t* sample_data;
    uint32_t data_length; // bytes readable at sample_data (sample can be switched without it)

    uint16_t volume;

    int repeat_list_stack[SC_REPEAT_STACK_MAX];
    int repeat_list_stack_top;

    ScEnvelopeCommand envelope_command;
    uint16_t envelope_counter;
    int16_t envelope_volume;
    bool start_envelope_release;

    uint16_t repeat_list_values[SC_REPEAT_LIST_MAX];

    // Hardware state
    int8_t* hw_sample_data;
    uint32_t hw_data_length; // bytes readable at hw_sample_data
    uint32_t hw_start_offset;
    uint16_t hw_sample_length;
} ScVoiceInfo40_50;

typedef struct ScGlobalInfo40_50 {
    uint16_t song_position;
    uint16_t speed;
    uint16_t max_speed;
    uint16_t speed_counter;
    uint16_t channel_counter;
} ScGlobalInfo40_50;

// What one Paula channel is set to play: the registers the tick writes, handed to the voice at its end.
typedef struct ScChannel {
    const int8_t* sample_data;
    uint32_t data_length; // bytes readable at sample_data
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint32_t sample_offset;
    uint16_t period;
    uint16_t volume;
    int32_t note;       // the note last started on this channel, -1 if none
    int32_t instrument; // likewise its sample or instrument number
    bool active;
    bool play_pending; // the sample above starts at the end of this tick
    // The repeat latch: taken over when the playing sample ends
    bool queue_pending;
    const int8_t* queued_data;
    uint32_t queued_length;
} ScChannel;

struct ScModule {
    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    ScModuleType module_type;
    ScModuleData module_data;

    uint16_t* periods; // calculated period table (8*16 entries)

    // Player type union
    bool is_3x;

    // 3.x player state
    ScSongInfo3x* song_info_3x;
    int num_songs_3x;
    bool is_version_32;
    uint16_t max_speed_counter;
    ScSongInfo3x* current_song_3x;
    ScGlobalInfo3x playing_info_3x;
    ScVoiceInfo3x voices_3x[6];

    // 4.0/5.0 player state
    ScSongInfo40_50* song_info_40_50;
    int num_songs_40_50;
    ScSongInfo40_50* current_song_40_50;
    ScGlobalInfo40_50 playing_info_40_50;
    ScVoiceInfo40_50 voices_40_50[6];

    uint32_t loop_count;

    // Mix channels
    ScChannel channels[SC_CHANNELS];
};

// True when the data is a Sound Control 3.x, 4.0 or 5.0 module.
bool sc_probe(const uint8_t* data, size_t size);

ScModule* sc_create(const uint8_t* data, size_t size);
void sc_destroy(ScModule* module);

int sc_subsong_count(const ScModule* module);
// (Re)start a subsong from its first position; the module plays on `voices`.
bool sc_start(ScModule* module, int subsong, NuadeVoices* voices);

// Run one player tick (SC_TICK_HZ) and hand the result to the voices. True when the song passed its loop point.
bool sc_tick(ScModule* module);

// Ticks between two track steps of `subsong` (rounded, >= 1), 0 for a bad subsong or a song that never steps.
uint32_t sc_ticks_per_step(const ScModule* module, int subsong);
ScVoiceState sc_voice_state(const ScModule* module, int channel);

// Edit API
int sc_get_instrument_count(const ScModule* module);
int sc_get_num_samples(const ScModule* module);
float sc_get_instrument_param(const ScModule* module, int inst, const char* param);
void sc_set_instrument_param(ScModule* module, int inst, const char* param, float value);
size_t sc_export(const ScModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
ScModule* sc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
