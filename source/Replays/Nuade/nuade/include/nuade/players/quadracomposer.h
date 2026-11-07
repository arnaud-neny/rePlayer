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

#define QC_CHANNELS 4
#define QC_TIMEBASE_HZ 709379

typedef struct QcModule QcModule;

// Display
typedef struct QcPosition {
    bool valid; // false before the first tick
    int position;
    int pattern;
    int row; // the row the last row tick played
    int speed;
    int tick; // tick within the row
} QcPosition;

typedef struct QcVoiceState {
    int note;   // the last note started on the channel, 1-36 (C-1 = 1), 0 if none
    int sample; // the sample number it plays, 1-based, 0 if none
} QcVoiceState;

typedef enum QcEffect {
    QC_EFFECT_ARPEGGIO = 0x0,
    QC_EFFECT_SLIDE_UP = 0x1,
    QC_EFFECT_SLIDE_DOWN = 0x2,
    QC_EFFECT_TONE_PORTAMENTO = 0x3,
    QC_EFFECT_VIBRATO = 0x4,
    QC_EFFECT_TONE_PORTA_VOL_SLIDE = 0x5,
    QC_EFFECT_VIBRATO_VOL_SLIDE = 0x6,
    QC_EFFECT_TREMOLO = 0x7,
    QC_EFFECT_SET_SAMPLE_OFFSET = 0x9,
    QC_EFFECT_VOLUME_SLIDE = 0xA,
    QC_EFFECT_POSITION_JUMP = 0xB,
    QC_EFFECT_SET_VOLUME = 0xC,
    QC_EFFECT_PATTERN_BREAK = 0xD,
    QC_EFFECT_EXTRA = 0xE,
    QC_EFFECT_SET_SPEED = 0xF
} QcEffect;

typedef struct QcTrackLine {
    uint8_t sample;
    int8_t note;
    QcEffect effect;
    uint8_t effect_arg;
} QcTrackLine;

typedef struct QcPattern {
    uint8_t number_of_rows;
    bool defined;        // listed in the EMIC pattern table
    QcTrackLine* tracks; // [4][number_of_rows+1] stored as flat array: tracks[ch * (number_of_rows+1) + row]
    int tracks_stride;   // number_of_rows + 1
} QcPattern;

typedef struct QcSample {
    uint32_t data_length; // bytes allocated for data
    uint32_t length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint8_t volume;
    uint8_t control_byte;
    uint8_t fine_tune;
    char name[21];
    int8_t* data;
} QcSample;

typedef struct QcChannelInfo {
    QcTrackLine track_line;
    uint32_t loop;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;
    uint32_t length;
    uint32_t start;
    int16_t note_nr;
    uint8_t shown_note;   // note_nr + 1 of the last note the channel started, 0 if none
    uint8_t shown_sample; // sample number (1-based) the channel plays, 0 if none
    uint16_t wanted_period;
    bool port_direction;
    uint8_t vibrato_wave;
    uint8_t glissando_control;
    uint8_t vibrato_command;
    uint16_t vibrato_position;
    uint8_t tremolo_wave;
    uint8_t tremolo_command;
    uint16_t tremolo_position;
    uint8_t sample_offset;
    uint8_t retrig;
    uint16_t port_speed;
    uint8_t fine_tune;
    int8_t* sample_data;
} QcChannelInfo;

typedef struct QcGlobalPlayingInfo {
    QcTrackLine* current_pattern; // points into a pattern's tracks array
    int current_pattern_stride;
    uint16_t current_position;
    uint16_t new_position;
    uint16_t break_row;
    uint16_t new_row;
    uint16_t row_count;
    uint16_t loop_row;
    uint8_t pattern_wait;

    uint16_t tempo;
    uint16_t speed;
    uint16_t speed_count;

    bool new_position_flag;
    bool jump_break_flag;
    uint8_t loop_count;
    bool intro_row;

    bool set_tempo;
} QcGlobalPlayingInfo;

typedef enum QcVoiceAction {
    QC_ACTION_NONE = 0,
    QC_ACTION_PLAY,
    QC_ACTION_STOP,
} QcVoiceAction;

// Paula registers of one channel: written during a tick, handed to the voice at its end.
typedef struct QcChannel {
    int8_t* sample_data;
    uint32_t sample_length;
    uint32_t sample_offset;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;      // 0..64
    QcVoiceAction action; // what this tick did to the sample; the last call wins
} QcChannel;

struct QcModule {
    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;

    char title[21];

    uint8_t start_tempo;

    uint8_t number_of_samples;
    QcSample* samples; // [256]

    uint8_t number_of_patterns;
    QcPattern* patterns; // [256]

    uint8_t number_of_positions;
    uint8_t* position_list;

    QcGlobalPlayingInfo playing_info;
    QcChannelInfo channel_info[4];
    QcChannel channels[4];

    bool end_reached;
    uint32_t loop_count;

    uint32_t tick_length; // CIA cycles until the next tick

    // Position visit tracking
    uint8_t visited[256];
};

// True when the data carries a Quadra Composer (EMOD) module header.
bool qc_probe(const uint8_t* data, size_t size);

QcModule* qc_create(const uint8_t* data, size_t size);
void qc_destroy(QcModule* module);

int qc_subsong_count(const QcModule* module);
// (Re)start from the first position; the channels play on `voices`, one per Paula channel.
bool qc_start(QcModule* module, int subsong, NuadeVoices* voices);

// Runs one player tick and updates the voices. True when the song ran back into a position it had already played.
bool qc_tick(QcModule* module);
// CIA cycles (PAL, QC_TIMEBASE_HZ) from the last tick to the next.
uint32_t qc_tick_length(const QcModule* module);

QcPosition qc_position(const QcModule* module);
QcVoiceState qc_voice_state(const QcModule* module, int voice);
// The pattern at a song position, -1 outside the song.
int qc_position_pattern(const QcModule* module, int position);

const char* qc_title(const QcModule* module);
// Sample name by slot (0-based); empty when the slot is unused.
const char* qc_sample_name(const QcModule* module, int index);

// Edit API
int qc_get_instrument_count(const QcModule* module);
int qc_get_num_patterns(const QcModule* module);
int qc_get_pattern_rows(const QcModule* module, int pattern);
int qc_get_num_positions(const QcModule* module);

void qc_get_cell(const QcModule* module, int pattern, int row, int channel, uint8_t* sample, int8_t* note,
                 uint8_t* effect, uint8_t* effect_arg);
void qc_set_cell(QcModule* module, int pattern, int row, int channel, uint8_t sample, int8_t note, uint8_t effect,
                 uint8_t effect_arg);

float qc_get_instrument_param(const QcModule* module, int inst, const char* param);
void qc_set_instrument_param(QcModule* module, int inst, const char* param, float value);

size_t qc_export(const QcModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
QcModule* qc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
