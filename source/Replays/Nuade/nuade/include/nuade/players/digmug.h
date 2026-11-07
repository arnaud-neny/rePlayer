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

typedef struct DmModule DmModule;

typedef struct DmPosition {
    int order; // sequence position
    int row;
    int rows;  // rows of the pattern being played (the pattern length effect changes it)
    int speed; // ticks per row
} DmPosition;

typedef struct DmVoiceState {
    int note;       // note of the last row that started a sample on the channel, with the position's transpose
                    // added (index into the period table past its 7 leading entries), -1 if none
    int instrument; // its instrument, 0-based, -1 if none
} DmVoiceState;

// One track row as stored.
typedef struct DmPatternCell {
    uint8_t note;       // 0 = none
    uint8_t instrument; // 1-based, 0 = none
    uint8_t effect;
    uint8_t effect_arg;
} DmPatternCell;

typedef enum DmModuleType {
    DM_TYPE_UNKNOWN = 0,
    DM_TYPE_DIGITAL_MUGICIAN = 1,
    DM_TYPE_DIGITAL_MUGICIAN2 = 2
} DmModuleType;

typedef enum DmEffect {
    DM_EFFECT_NONE = 0,
    DM_EFFECT_PITCH_BEND = 1,
    DM_EFFECT_NO_INSTRUMENT_EFFECT = 2,
    DM_EFFECT_NO_INSTRUMENT_VOLUME = 3,
    DM_EFFECT_NO_INSTRUMENT_EFFECT_AND_VOLUME = 4,
    DM_EFFECT_PATTERN_LENGTH = 5,
    DM_EFFECT_SONG_SPEED = 6,
    DM_EFFECT_FILTER_ON = 7,
    DM_EFFECT_FILTER_OFF = 8,
    DM_EFFECT_SWITCH_FILTER = 9,
    DM_EFFECT_NO_DMA = 10,
    DM_EFFECT_ARPEGGIO = 11,
    DM_EFFECT_NO_WANDER = 12,
    DM_EFFECT_SHUFFLE = 13
} DmEffect;

typedef enum DmInstrumentEffect {
    DM_INST_EFFECT_NONE = 0,
    DM_INST_EFFECT_FILTER = 1,
    DM_INST_EFFECT_MIXING = 2,
    DM_INST_EFFECT_SCR_LEFT = 3,
    DM_INST_EFFECT_SCR_RIGHT = 4,
    DM_INST_EFFECT_UPSAMPLE = 5,
    DM_INST_EFFECT_DOWNSAMPLE = 6,
    DM_INST_EFFECT_NEGATE = 7,
    DM_INST_EFFECT_MAD_MIX1 = 8,
    DM_INST_EFFECT_ADDITION = 9,
    DM_INST_EFFECT_FILTER2 = 10,
    DM_INST_EFFECT_MORPHING = 11,
    DM_INST_EFFECT_MORPH_F = 12,
    DM_INST_EFFECT_FILTER3 = 13,
    DM_INST_EFFECT_POLYGATE = 14,
    DM_INST_EFFECT_COLGATE = 15
} DmInstrumentEffect;

typedef struct DmSubSong {
    bool loop_song;
    uint8_t loop_position;
    uint8_t song_speed;
    uint8_t number_of_sequences;
    char name[13];
} DmSubSong;

typedef struct DmSequence {
    uint8_t track_number;
    int8_t transpose;
} DmSequence;

typedef struct DmTrackRow {
    uint8_t note;
    uint8_t instrument;
    uint8_t effect;
    uint8_t effect_param;
} DmTrackRow;

typedef struct DmTrack {
    DmTrackRow rows[64];
} DmTrack;

typedef struct DmInstrument {
    uint8_t waveform_number; // >= 32 -> sample number
    uint16_t loop_length;
    uint8_t finetune;
    uint8_t arpeggio_number;
    uint8_t volume;
    uint8_t volume_speed;
    bool volume_loop;
    uint8_t pitch;
    uint8_t pitch_speed;
    uint8_t pitch_loop;
    uint8_t delay;
    DmInstrumentEffect effect;
    uint8_t effect_speed;
    uint8_t effect_index;
    uint8_t source_wave1;
    uint8_t source_wave2;
} DmInstrument;

typedef struct DmSample {
    uint32_t start_offset;
    uint32_t end_offset;
    int32_t loop_start;
    int8_t* sample_data;
} DmSample;

typedef struct DmVoiceInfo {
    uint16_t track;
    int16_t transpose;
    uint16_t last_note;
    uint16_t last_instrument;
    int shown_note;       // note of the last row that started a sample, with the position's transpose; -1 if none
    int shown_instrument; // its instrument, -1 if none
    DmEffect last_effect;
    uint16_t last_effect_param;
    uint16_t fine_tune;

    uint16_t note_period;

    uint16_t pitch_bend_end_note;
    uint16_t pitch_bend_end_period;
    int16_t current_pitch_bend_period;

    uint16_t pitch_index;
    uint16_t arpeggio_index;

    uint16_t volume_index;
    uint16_t volume_speed_counter;

    uint8_t instrument_delay;
    uint8_t instrument_effect_speed;
} DmVoiceInfo;

// Paula registers of one channel: written during a tick, handed to the voice at its end
typedef struct DmChannel {
    const int8_t* sample_data;
    uint32_t sample_offset;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length; // 0 = one-shot
    uint16_t volume;      // Amiga volume 0-64
    uint32_t period;
    bool active;
    bool muted;
    bool play_pending;  // a new sample starts
    bool latch_pending; // the loop registers were rewritten while the old sample keeps playing
} DmChannel;

struct DmModule {
    DmModuleType module_type;
    int number_of_channels;

    uint16_t number_of_tracks;
    uint32_t number_of_instruments;
    uint32_t number_of_waveforms;
    uint32_t number_of_samples;

    // Sub-songs
    int sub_song_count;    // number of playable sub-songs
    int* sub_song_mapping; // maps playable index to real sub-song index
    uint32_t sub_song_sequence_length[8];
    DmSubSong sub_songs[8];

    // Sequences: sub_song_sequences[sub_song][channel * seq_len + position]
    DmSequence* sub_song_sequences[8]; // each is [4 * seq_len]

    DmTrack* tracks;
    uint8_t arpeggios[8][32];

    DmInstrument* instruments;
    int8_t** waveforms; // each is 128 bytes
    DmSample* samples;

    // Playing state
    uint16_t speed;
    uint16_t current_speed;
    uint16_t last_shown_speed;
    bool new_pattern;
    bool new_row;
    uint16_t current_position;
    uint16_t song_length;
    uint16_t current_row;
    uint16_t pattern_length;
    // The row played by the last tick: the position and row it was played at, and the pattern length it left.
    uint16_t shown_position;
    uint16_t shown_row;
    uint16_t shown_rows;

    DmVoiceInfo voice_info[7]; // max 7 channels
    DmChannel channels[7];

    int sub_song_number;
    DmSubSong* current_sub_song;
    DmSequence* current_sequence;  // points into sub_song_sequences
    uint32_t current_sequence_len; // length of this sequence
    DmSequence* current_sequence2; // for DMU2 second sub-song
    uint32_t current_sequence2_len;

    uint8_t ch_tab[4];
    int ch_tab_index;

    int16_t* instrument_to_sample_info_mapping;

    // Times the song wrapped past its last position
    uint32_t loop_count;

    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    size_t file_size; // the size of the loaded file, independent of the export copy

    // Amiga filter state (low-pass)
    bool amiga_filter;
};

// True when the data carries a Digital Mugician (1 or 2) module mark.
bool dm_probe(const uint8_t* data, size_t size);

DmModule* dm_create(const uint8_t* data, size_t size);
void dm_destroy(DmModule* module);

int dm_subsong_count(const DmModule* module);
// (Re)start a subsong from its first position; the player drives `voices` from then on.
bool dm_start(DmModule* module, int subsong, NuadeVoices* voices);

// Tick timing: a tick lasts dm_tick_length() units of dm_timebase_hz(). Digital Mugician 1 ticks once per VBlank;
// Digital Mugician 2 ticks when its mix buffer has played out.
uint32_t dm_timebase_hz(const DmModule* module);
uint32_t dm_tick_length(const DmModule* module);

// 4 for Digital Mugician, 7 for Digital Mugician 2.
int dm_channel_count(const DmModule* module);
void dm_set_channel_mask(DmModule* module, uint32_t mask);

// Run one player tick and set the voices. Returns true when the song passed its loop point during the tick.
bool dm_tick(DmModule* module);

// The running subsong's name; empty when it has none.
const char* dm_title(const DmModule* module);
const char* dm_format_name(const DmModule* module);

// Edit API
int dm_get_instrument_count(const DmModule* module);
int dm_get_num_tracks(const DmModule* module);
void dm_get_cell(const DmModule* module, int track_idx, int row, uint8_t* note, uint8_t* instrument, uint8_t* effect,
                 uint8_t* effect_arg);
void dm_set_cell(DmModule* module, int track_idx, int row, uint8_t note, uint8_t instrument, uint8_t effect,
                 uint8_t effect_arg);
float dm_get_instrument_param(const DmModule* module, int inst, const char* param);
void dm_set_instrument_param(DmModule* module, int inst, const char* param, float value);
size_t dm_export(const DmModule* module, uint8_t* out, size_t max_size);

// The row played by the last tick (position 0, row 0 before the first one).
DmPosition dm_position(const DmModule* module);
// Positions of the running subsong.
int dm_position_count(const DmModule* module);

DmVoiceState dm_voice_state(const DmModule* module, int channel);

// `position` 0 .. dm_position_count - 1 of the running subsong, `row` 0..63.
DmPatternCell dm_pattern_cell(const DmModule* module, int position, int channel, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
DmModule* dm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
