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

// A tick lasts 2.5 / tempo seconds: ACT_TICK_LENGTH units of act_timebase_hz() (2 * tempo).
#define ACT_TICK_LENGTH 5

typedef struct ActModule ActModule;

// Display
typedef struct ActPosition {
    int position; // index into the position lists of the row last read
    int row;
    int rows; // rows per pattern; a set rows effect changes it for the rest of the song
    int speed;
    int tick; // ticks since the row
} ActPosition;

typedef struct ActVoiceState {
    int note;       // period table index the last started note plays (1-72), 0 if none
    int instrument; // 1-based instrument of that note, 0 if none
} ActVoiceState;

// A track entry as written in the module; 0 in a field the entry leaves empty.
typedef struct ActCell {
    uint8_t note;
    uint8_t instrument;
    uint8_t effect; // 0x70-0x7f
    uint8_t effect_arg;
} ActCell;

typedef enum ActEffect {
    ACT_EFFECT_NONE = 0x00,
    ACT_EFFECT_ARPEGGIO = 0x70,
    ACT_EFFECT_SLIDE_UP = 0x71,
    ACT_EFFECT_SLIDE_DOWN = 0x72,
    ACT_EFFECT_VOLUME_SLIDE_AFTER_ENVELOPE = 0x73,
    ACT_EFFECT_VIBRATO = 0x74,
    ACT_EFFECT_SET_ROWS = 0x75,
    ACT_EFFECT_SET_SAMPLE_OFFSET = 0x76,
    ACT_EFFECT_NOTE_DELAY = 0x77,
    ACT_EFFECT_MUTE = 0x78,
    ACT_EFFECT_SAMPLE_RESTART = 0x79,
    ACT_EFFECT_TREMOLO = 0x7a,
    ACT_EFFECT_BREAK = 0x7b,
    ACT_EFFECT_SET_VOLUME = 0x7c,
    ACT_EFFECT_VOLUME_SLIDE = 0x7d,
    ACT_EFFECT_VOLUME_SLIDE_AND_VIBRATO = 0x7e,
    ACT_EFFECT_SET_SPEED = 0x7f
} ActEffect;

typedef enum ActEnvelopeState {
    ACT_ENV_DONE = 0,
    ACT_ENV_ATTACK = 1,
    ACT_ENV_DECAY = 2,
    ACT_ENV_SUSTAIN = 3,
    ACT_ENV_RELEASE = 4
} ActEnvelopeState;

typedef struct ActInstrumentList {
    uint8_t list_number;
    uint8_t number_of_values_in_list;
    uint8_t start_counter_delta_value;
    uint8_t counter_end_value;
} ActInstrumentList;

typedef struct ActInstrument {
    ActInstrumentList sample_number_list;
    ActInstrumentList arpeggio_list;
    ActInstrumentList frequency_list;

    int8_t portamento_increment;
    uint8_t portamento_delay;

    int8_t note_transpose;

    uint8_t attack_end_volume;
    uint8_t attack_speed;
    uint8_t decay_end_volume;
    uint8_t decay_speed;
    uint8_t sustain_delay;
    uint8_t release_end_volume;
    uint8_t release_speed;
} ActInstrument;

typedef struct ActSample {
    char name[33];
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
    int8_t* sample_data;

    uint8_t arpeggio_list_number;
    uint16_t effect_start_position;
    uint16_t effect_length;
    uint16_t effect_speed;
    uint16_t effect_mode;

    uint16_t counter_init_value;
} ActSample;

typedef struct ActSampleExtra {
    int8_t* modified_sample_data;

    int16_t effect_increment_value;
    int32_t effect_position;
    uint16_t effect_speed_counter;
    bool already_taken;
} ActSampleExtra;

typedef struct ActSinglePositionInfo {
    uint8_t track_number;
    int8_t note_transpose;
    int8_t instrument_transpose;
} ActSinglePositionInfo;

typedef struct ActSongInfo {
    uint8_t start_position;
    uint8_t end_position;
    uint8_t loop_position;
    uint8_t speed;
} ActSongInfo;

typedef struct ActVoiceInfo {
    ActSinglePositionInfo* position_list;
    const uint8_t* track_data;
    int track_size;
    int track_position;
    uint8_t delay_counter;

    uint16_t instrument_number;
    ActInstrument* instrument;
    int8_t instrument_transpose;

    uint16_t sample_number;
    int8_t* sample_data;
    uint32_t sample_data_size; // bytes behind sample_data
    uint32_t sample_offset;
    uint16_t sample_length;
    uint32_t sample_loop_start;
    uint16_t sample_loop_length;

    uint16_t note;
    int8_t note_transpose;
    uint16_t note_period;

    uint16_t final_note;
    uint16_t final_period;
    int16_t final_volume;
    uint16_t global_voice_volume;

    ActEnvelopeState envelope_state;
    uint16_t sustain_counter;

    uint8_t sample_number_list_speed_counter;
    int16_t sample_number_list_position;

    uint8_t arpeggio_list_speed_counter;
    int16_t arpeggio_list_position;

    uint8_t frequency_list_speed_counter;
    int16_t frequency_list_position;

    ActEffect effect;
    uint16_t effect_argument;

    uint8_t portamento_delay_counter;
    int16_t portamento_value;

    uint16_t tone_portamento_end_period;
    int16_t tone_portamento_increment_value;

    uint8_t vibrato_effect_argument;
    int8_t vibrato_table_index;

    uint8_t tremolo_effect_argument;
    int8_t tremolo_table_index;
    uint16_t tremolo_volume; // Never set in original (bug in original player)

    uint16_t sample_offset_effect_argument;
    uint16_t note_delay_counter;

    uint16_t restart_delay_counter;
    int8_t* restart_sample_data;
    uint32_t restart_sample_data_size;
    uint32_t restart_sample_offset;
    uint16_t restart_sample_length;

    bool trig_sample;

    // For display: the note (period table index) and instrument (1-based) the last started note plays; 0 = none
    int shown_note;
    int shown_instrument;
} ActVoiceInfo;

// Paula-style registers of one channel; the tick writes them and act_sync_voice hands them to the voice
typedef struct ActChannel {
    int8_t* sample_data;
    uint32_t data_size; // bytes behind sample_data; reads never go past it
    uint32_t sample_offset;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume; // 0-64
    int16_t sample_number;
    bool play_pending;
    // For SetSample / SetLoop (deferred triggers)
    bool has_pending_sample;
    int8_t* pending_sample_data;
    uint32_t pending_sample_offset;
    uint32_t pending_sample_length;
    bool has_pending_loop;
    uint32_t pending_loop_start;
    uint32_t pending_loop_length;
} ActChannel;

struct ActModule {
    NuadeVoices* paula;
    bool timer_started; // the first timer interrupt comes one tick after the start

    uint32_t loop_count;

    uint16_t tempo;

    ActSongInfo* song_info_list;
    int num_song_infos;

    ActSinglePositionInfo** positions; // [4][count]
    int position_count;

    ActInstrument* instruments;
    int num_instruments;

    ActSample* samples;
    int num_samples;

    ActSampleExtra* sample_extras;

    int8_t** sample_number_list; // [count][16]
    int num_sample_number_lists;

    int8_t** arpeggio_list; // [count][16]
    int num_arpeggio_lists;

    int8_t** frequency_list; // [count][16]
    int num_frequency_lists;

    uint8_t** tracks;
    int* track_sizes;
    int num_tracks;

    ActSongInfo* current_song_info;

    // Playing state
    uint8_t speed_counter;
    uint8_t current_speed;
    uint8_t measure_counter;
    uint8_t current_position;
    uint8_t loop_position;
    uint8_t end_position;
    uint8_t current_row_position;
    uint8_t number_of_rows;

    // For display: where the row last read sits. The counters above already point past it.
    uint8_t shown_position;
    uint8_t shown_row;
    uint8_t shown_rows;

    ActVoiceInfo voices[4];

    ActChannel channels[4];
};

// True when the data carries the Actionamics signature.
bool act_probe(const uint8_t* data, size_t size);

ActModule* act_create(const uint8_t* data, size_t size);
void act_destroy(ActModule* module);

int act_subsong_count(const ActModule* module);
uint32_t act_timebase_hz(const ActModule* module);

// (Re)start a subsong from its first position; the player drives `voices` from then on.
bool act_start(ActModule* module, int subsong, NuadeVoices* voices);

// One player tick: runs the song and hands the channel registers to the voices. True when the song passed its loop
// point during the tick. The first call after act_start only arms the timer.
bool act_tick(ActModule* module);

// Sample names, for display.
int act_get_sample_count(const ActModule* module);
const char* act_get_sample_name(const ActModule* module, int samp);

ActPosition act_position(const ActModule* module);
ActVoiceState act_voice_state(const ActModule* module, int voice);
int act_position_count(const ActModule* module);
// The entry of `channel` at `row` of `position`; all 0 outside the song.
ActCell act_pattern_cell(const ActModule* module, int position, int channel, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
ActModule* act_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
