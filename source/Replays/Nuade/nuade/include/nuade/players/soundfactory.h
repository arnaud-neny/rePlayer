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

// The player's tick rate: one sf_tick per VBlank.
#define SF_TICK_HZ 50

// Working buffer of the phasing and filter effects.
#define SF_PHASING_SIZE 256
#define SF_MAX_STACK 64
// Instrument lookup: simple array indexed by offset
#define SF_MAX_INSTRUMENTS 256

typedef struct SfModule SfModule;

typedef enum SfTickEvents {
    SfTick_None = 0,
    SfTick_Looped = 1, // every voice has reached a Loop opcode (or is silent) since the last such pass
    SfTick_Ended = 2,  // an End opcode without a fade-out, or a fade-out reached silence
} SfTickEvents;

typedef enum SfEnvState { SF_ENV_ATTACK = 0, SF_ENV_DECAY = 1, SF_ENV_SUSTAIN = 2, SF_ENV_RELEASE = 3 } SfEnvState;

typedef struct SfInstrument {
    int16_t instrument_number;
    uint16_t sample_length;
    uint16_t sampling_period;
    uint8_t effect_byte; // SfInstrFlag
    uint8_t tremolo_speed;
    uint8_t tremolo_step;
    uint8_t tremolo_range;
    uint16_t portamento_step;
    uint8_t portamento_speed;
    uint8_t arpeggio_speed;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    int8_t vibrato_step;
    uint8_t vibrato_amount;
    uint8_t attack_time;
    uint8_t decay_time;
    uint8_t sustain_level;
    uint8_t release_time;
    uint8_t phasing_start;
    uint8_t phasing_end;
    uint8_t phasing_speed;
    int8_t phasing_step;
    uint8_t wave_count;
    uint8_t octave;
    uint8_t filter_frequency;
    uint8_t filter_end;
    uint8_t filter_speed;
    uint16_t dasr_sustain_offset;
    uint16_t dasr_release_offset;
    int8_t* sample_data;
} SfInstrument;

typedef struct SfVoiceInfo {
    int channel_number;
    bool voice_enabled;

    uint32_t start_position;
    uint32_t current_position;

    uint8_t current_instrument;

    uint16_t note_duration;
    uint16_t note_duration2;
    uint8_t note;
    int8_t transpose;

    uint8_t fine_tune;
    uint16_t period;

    uint8_t current_volume;
    uint8_t volume;

    uint16_t active_period;
    uint8_t portamento_counter;

    bool arpeggio_flag;
    uint8_t arpeggio_counter;

    uint8_t vibrato_delay;
    uint8_t vibrato_counter;
    uint8_t vibrato_counter2;
    int16_t vibrato_relative;
    int8_t vibrato_step;

    uint8_t tremolo_counter;
    int8_t tremolo_step;
    uint8_t tremolo_volume;

    SfEnvState envelope_state;
    uint16_t envelope_counter;

    uint8_t phasing_counter;
    int8_t phasing_step;
    int8_t phasing_relative;

    uint8_t filter_counter;
    int8_t filter_step;
    uint8_t filter_relative;

    uint32_t stack[SF_MAX_STACK];
    int stack_top;

    uint8_t note_start_flag;
    uint8_t note_start_flag1;

    int8_t phasing_buffer[SF_PHASING_SIZE];
} SfVoiceInfo;

typedef struct SfSongInfo {
    uint8_t enabled_channels;
    uint32_t opcode_start_offsets[4];
} SfSongInfo;

typedef struct SfChannel {
    bool active;
    bool muted;
    int8_t* sample_data;
    uint32_t sample_offset;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;
    bool play_pending; // a new sample starts at the end of this tick
    int note;          // the note and instrument of the sample last started, -1 before the first
    int instrument;
} SfChannel;

struct SfModule {
    NuadeVoices* nuade_voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    SfSongInfo* song_infos;
    int num_songs;

    uint8_t* opcodes;
    uint32_t opcodes_length;

    // Original instruments (keyed by offset)
    SfInstrument original_instruments[SF_MAX_INSTRUMENTS];
    uint32_t original_instrument_offsets[SF_MAX_INSTRUMENTS];
    int num_original_instruments;

    // Playing state
    SfInstrument default_instrument;
    SfInstrument* sound_table[256]; // pointers into playing_instruments
    SfInstrument playing_instruments[SF_MAX_INSTRUMENTS];
    int num_playing_instruments;

    bool fade_out_flag;
    bool fade_in_flag;
    uint8_t fade_out_volume;
    uint8_t fade_out_counter;
    uint8_t fade_out_speed;
    uint8_t request_counter;

    SfVoiceInfo voices[4];
    SfChannel channels[4];

    SfSongInfo* current_song;
    bool has_ended;     // set during the tick that ended the song
    int ended_channels; // voices that reached Loop/End (or are disabled) in the current pass
    uint32_t loop_count;
};

// True when the data has the layout of a Sound Factory module (the format has no magic number).
bool sf_probe(const uint8_t* data, size_t size);

SfModule* sf_create(const uint8_t* data, size_t size);
void sf_destroy(SfModule* module);

int sf_subsong_count(const SfModule* module);
// (Re)start a subsong from its beginning; the player drives `voices` from sf_tick.
bool sf_start(SfModule* module, int subsong, NuadeVoices* voices);

int sf_channel_count(const SfModule* module);
void sf_set_channel_mask(SfModule* module, uint32_t mask);

// Run one player tick and set the voices.
SfTickEvents sf_tick(SfModule* module);

// The note (after the channel's transpose) and instrument number of the sample last started on a channel; false if
// none was started yet.
bool sf_voice_last_note(const SfModule* module, int channel, int* note, int* instrument);

// Edit API
int sf_get_instrument_count(const SfModule* module);
float sf_get_instrument_param(const SfModule* module, int inst, const char* param);
void sf_set_instrument_param(SfModule* module, int inst, const char* param, float value);
size_t sf_export(const SfModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
SfModule* sf_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
