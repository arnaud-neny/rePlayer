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

#define DSS_MIN_SIZE 0x61e

// Unit of the tick length; the tempo is a BPM, so a tick lasts 2.5 / BPM seconds.
#define DSS_TIMEBASE_HZ 709379

// Period of a note that cuts the voice.
#define DSS_PERIOD_CUT 0x7ff

#define DSS_MAX_VISITED 4096

typedef struct DssModule DssModule;

typedef struct DssPosition {
    int position; // index in the position list
    int row;
    int speed; // ticks per row
    int tempo; // BPM
} DssPosition;

typedef struct DssVoiceState {
    uint16_t period; // of the last note started on the voice, 0 if none
    uint8_t sample;  // its sample number (1..31), 0 if none
} DssVoiceState;

typedef struct DssCell {
    uint16_t period;
    uint8_t sample;
    uint8_t effect;
    uint8_t effect_arg;
} DssCell;

typedef enum DssEffect {
    DSS_EFF_ARPEGGIO = 0x00,
    DSS_EFF_SLIDE_UP = 0x01,
    DSS_EFF_SLIDE_DOWN = 0x02,
    DSS_EFF_SET_VOLUME = 0x03,
    DSS_EFF_SET_MASTER_VOLUME = 0x04,
    DSS_EFF_SET_SONG_SPEED = 0x05,
    DSS_EFF_POSITION_JUMP = 0x06,
    DSS_EFF_SET_FILTER = 0x07,
    DSS_EFF_PITCH_UP = 0x08,
    DSS_EFF_PITCH_DOWN = 0x09,
    DSS_EFF_PITCH_CONTROL = 0x0A,
    DSS_EFF_SET_SONG_TEMPO = 0x0B,
    DSS_EFF_VOLUME_UP = 0x0C,
    DSS_EFF_VOLUME_DOWN = 0x0D,
    DSS_EFF_VOLUME_SLIDE_UP = 0x0E,
    DSS_EFF_VOLUME_SLIDE_DOWN = 0x0F,
    DSS_EFF_MASTER_VOL_UP = 0x10,
    DSS_EFF_MASTER_VOL_DOWN = 0x11,
    DSS_EFF_MASTER_VOL_SLIDE_UP = 0x12,
    DSS_EFF_MASTER_VOL_SLIDE_DOWN = 0x13,
    DSS_EFF_SET_LOOP_START = 0x14,
    DSS_EFF_JUMP_TO_LOOP = 0x15,
    DSS_EFF_RETRIG_NOTE = 0x16,
    DSS_EFF_NOTE_DELAY = 0x17,
    DSS_EFF_NOTE_CUT = 0x18,
    DSS_EFF_SET_SAMPLE_OFFSET = 0x19,
    DSS_EFF_SET_FINE_TUNE = 0x1A,
    DSS_EFF_PORTAMENTO = 0x1B,
    DSS_EFF_PORTA_VOL_SLIDE_UP = 0x1C,
    DSS_EFF_PORTA_VOL_SLIDE_DOWN = 0x1D,
    DSS_EFF_PORTAMENTO_CONTROL = 0x1E
} DssEffect;

typedef struct DssSample {
    char name[31];
    uint32_t start_offset;
    uint16_t length;
    uint32_t loop_start;
    uint16_t loop_length;
    uint8_t fine_tune;
    uint8_t volume;
    uint16_t frequency;
    int8_t* data;
    uint32_t data_size; // bytes allocated at `data`; playback never reads past it
} DssSample;

typedef struct DssTrackLine {
    uint8_t sample;
    uint16_t period;
    DssEffect effect;
    uint8_t effect_arg;
} DssTrackLine;

typedef struct DssPattern {
    DssTrackLine tracks[4][64];
} DssPattern;

typedef struct DssVoiceInfo {
    uint8_t sample;
    uint16_t period;
    DssEffect effect;
    uint8_t effect_arg;

    uint8_t fine_tune;
    uint8_t volume;

    uint8_t playing_sample_number;
    int8_t* sample_data;
    uint32_t sample_data_size;
    uint32_t sample_start_offset;
    uint16_t sample_length;
    uint32_t loop_start;
    uint16_t loop_length;
    uint16_t sample_offset;

    uint16_t pitch_period;
    bool portamento_direction;
    uint8_t portamento_speed;
    uint16_t portamento_end_period;

    bool use_tone_porta_for_slide;
    bool use_tone_porta_for_porta;

    int16_t loop_row;
    uint16_t loop_counter;

    uint16_t started_period; // period and sample number of the last note the rows started on the voice
    uint8_t started_sample;
} DssVoiceInfo;

typedef struct DssGlobalInfo {
    uint8_t current_tempo;
    uint8_t current_speed;
    uint8_t speed_counter;

    uint16_t current_position;
    int16_t current_row;
    uint16_t shown_position; // the position and row dss_play_next_row played last
    uint16_t shown_row;
    bool position_jump;
    uint16_t new_position;

    bool set_loop_row;
    int16_t loop_row;

    uint16_t inverse_master_volume;
    uint16_t next_retrig_tick;
    uint16_t arpeggio_counter;
} DssGlobalInfo;

// What a Paula channel would hold: handed to the voice at the end of every tick.
typedef struct DssChannel {
    bool active;
    bool play_pending; // a new sample starts at the end of this tick

    int8_t* sample_data;
    uint32_t sample_data_size;
    uint32_t sample_offset;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;

    uint16_t period;
    uint16_t volume;

    NuadeSample handed; // what the voice was last given
} DssChannel;

struct DssModule {
    NuadeVoices* nuade_voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    uint8_t start_tempo;
    uint8_t start_speed;

    DssSample samples[31];
    uint8_t* positions;
    int num_positions;
    DssPattern* patterns;
    int num_patterns;

    DssGlobalInfo playing_info;
    DssVoiceInfo voices[4];
    DssChannel channels[4];

    uint32_t loop_count;
    uint32_t tick_length; // in DSS_TIMEBASE_HZ units

    uint8_t visited[DSS_MAX_VISITED / 8];
};

// True when the data carries the module mark and is large enough for the fixed header.
bool dss_probe(const uint8_t* data, size_t size);

DssModule* dss_create(const uint8_t* data, size_t size);
void dss_destroy(DssModule* module);

int dss_subsong_count(const DssModule* module);
// (Re)start from the first position; the module sets its voices through `voices` from then on.
bool dss_start(DssModule* module, int subsong, NuadeVoices* voices);

// Run one player tick and set the voices. `*length` is the time to the next tick in DSS_TIMEBASE_HZ units. Returns true
// when the song ran back into a position it had already played during this tick.
bool dss_tick(DssModule* module, uint32_t* length);

const char* dss_format_name(const DssModule* module);

// The row whose notes were started by the last tick.
DssPosition dss_position(const DssModule* module);

DssVoiceState dss_voice_state(const DssModule* module, int channel);

// `position` 0 .. dss_get_num_positions - 1, `channel` 0..3, `row` 0..63.
DssCell dss_pattern_cell(const DssModule* module, int position, int channel, int row);

// Edit API
int dss_get_instrument_count(const DssModule* module);
int dss_get_num_patterns(const DssModule* module);
int dss_get_num_positions(const DssModule* module);
/** Live play head as (position << 8) | row, or -1 with no module. */
int dss_get_play_position(const DssModule* module);

void dss_get_cell(const DssModule* module, int pattern, int row, int channel, uint8_t* sample, uint16_t* period,
                  uint8_t* effect, uint8_t* effect_arg);
void dss_set_cell(DssModule* module, int pattern, int row, int channel, uint8_t sample, uint16_t period, uint8_t effect,
                  uint8_t effect_arg);

const char* dss_get_instrument_name(const DssModule* module, int inst);
float dss_get_instrument_param(const DssModule* module, int inst, const char* param);
void dss_set_instrument_param(DssModule* module, int inst, const char* param, float value);

size_t dss_export(const DssModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
DssModule* dss_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
