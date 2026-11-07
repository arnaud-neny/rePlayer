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

// CIA ticks per second; ftm_tick_length() is in these units.
#define FTM_TIMEBASE_HZ 709379

#define FTM_MAX_SAMPLES 64
#define FTM_MAX_SELS 64
#define FTM_MAX_VISITED 4096
#define FTM_MAX_PAT_LOOPS 16

typedef struct FtmModule FtmModule;

// Display
typedef struct FtmPosition {
    int measure; // the row last handled is `row` of this measure
    int row;
    int speed; // ticks per row
    int tick;  // ticks since the row
} FtmPosition;

typedef struct FtmVoiceState {
    int note;   // the track note (1-34) last started on the voice, 0 if none
    int sample; // 0-based sample it plays, -1 if none
} FtmVoiceState;

typedef enum FtmTrackEffect {
    FTM_TE_NONE = 0,
    FTM_TE_VOLUME0,
    FTM_TE_VOLUME1,
    FTM_TE_VOLUME2,
    FTM_TE_VOLUME3,
    FTM_TE_VOLUME4,
    FTM_TE_VOLUME5,
    FTM_TE_VOLUME6,
    FTM_TE_VOLUME7,
    FTM_TE_VOLUME8,
    FTM_TE_VOLUME9,
    FTM_TE_SEL_EFFECT,
    FTM_TE_PORTAMENTO,
    FTM_TE_VOLUME_DOWN,
    FTM_TE_PATTERN_LOOP,
    FTM_TE_SKIP_EMPTY
} FtmTrackEffect;

typedef enum FtmSoundEffect {
    FTM_SE_NOTHING = 0,
    FTM_SE_WAIT,
    FTM_SE_GOTO,
    FTM_SE_LOOP,
    FTM_SE_GOTO_SCRIPT,
    FTM_SE_END,
    FTM_SE_IF_PITCH_EQ,
    FTM_SE_IF_PITCH_LT,
    FTM_SE_IF_PITCH_GT,
    FTM_SE_IF_VOL_EQ,
    FTM_SE_IF_VOL_LT,
    FTM_SE_IF_VOL_GT,
    FTM_SE_ON_NEW_PITCH,
    FTM_SE_ON_NEW_VOL,
    FTM_SE_ON_NEW_SAMPLE,
    FTM_SE_ON_RELEASE,
    FTM_SE_ON_PORTAMENTO,
    FTM_SE_ON_VOL_DOWN,
    FTM_SE_PLAY_CUR_SAMPLE,
    FTM_SE_PLAY_QUIET,
    FTM_SE_PLAY_POS,
    FTM_SE_PLAY_POS_ADD,
    FTM_SE_PLAY_POS_SUB,
    FTM_SE_PITCH,
    FTM_SE_DETUNE,
    FTM_SE_DETUNE_ADD,
    FTM_SE_DETUNE_SUB,
    FTM_SE_VOLUME,
    FTM_SE_VOLUME_ADD,
    FTM_SE_VOLUME_SUB,
    FTM_SE_CUR_SAMPLE,
    FTM_SE_SAMPLE_START,
    FTM_SE_SAMPLE_START_ADD,
    FTM_SE_SAMPLE_START_SUB,
    FTM_SE_ONESHOT_LEN,
    FTM_SE_ONESHOT_LEN_ADD,
    FTM_SE_ONESHOT_LEN_SUB,
    FTM_SE_REPEAT_LEN,
    FTM_SE_REPEAT_LEN_ADD,
    FTM_SE_REPEAT_LEN_SUB,
    FTM_SE_GET_PITCH_OF,
    FTM_SE_GET_VOL_OF,
    FTM_SE_GET_SAMPLE_OF,
    FTM_SE_CLONE_TRACK,
    FTM_SE_LFO1_START,
    FTM_SE_LFO1_SPEED_ADD,
    FTM_SE_LFO1_SPEED_SUB,
    FTM_SE_LFO2_START,
    FTM_SE_LFO2_SPEED_ADD,
    FTM_SE_LFO2_SPEED_SUB,
    FTM_SE_LFO3_START,
    FTM_SE_LFO3_SPEED_ADD,
    FTM_SE_LFO3_SPEED_SUB,
    FTM_SE_LFO4_START,
    FTM_SE_LFO4_SPEED_ADD,
    FTM_SE_LFO4_SPEED_SUB,
    FTM_SE_WORK_ON_TRACK,
    FTM_SE_WORK_TRACK_ADD,
    FTM_SE_GLOBAL_VOLUME,
    FTM_SE_GLOBAL_SPEED,
    FTM_SE_TICKS_PER_LINE,
    FTM_SE_JUMP_TO_LINE
} FtmSoundEffect;

typedef enum FtmLfoTarget {
    FTM_LFO_NOTHING = 0,
    FTM_LFO_LFO1_SPEED,
    FTM_LFO_LFO2_SPEED,
    FTM_LFO_LFO3_SPEED,
    FTM_LFO_LFO4_SPEED,
    FTM_LFO_LFO1_DEPTH,
    FTM_LFO_LFO2_DEPTH,
    FTM_LFO_LFO3_DEPTH,
    FTM_LFO_LFO4_DEPTH,
    FTM_LFO_UNUSED9,
    FTM_LFO_TRACK_AMP,
    FTM_LFO_UNUSED11,
    FTM_LFO_UNUSED12,
    FTM_LFO_UNUSED13,
    FTM_LFO_UNUSED14,
    FTM_LFO_TRACK_FREQ
} FtmLfoTarget;

typedef struct FtmSample {
    int8_t* sample_data;
    uint16_t oneshot_length; // in words
    uint32_t loop_start;     // in bytes
    uint16_t loop_length;    // in words
    uint32_t total_length;   // in bytes
} FtmSample;

typedef struct FtmSelLine {
    FtmSoundEffect effect;
    uint8_t arg1;
    uint16_t arg2;
} FtmSelLine;

typedef struct FtmSelScript {
    FtmSelLine* lines;
    int num_lines;
} FtmSelScript;

typedef struct FtmTrackLine {
    FtmTrackEffect effect;
    uint16_t effect_arg;
    uint8_t note;
} FtmTrackLine;

typedef struct FtmTrack {
    uint16_t default_spacing;
    FtmTrackLine* lines;
    int num_lines;
} FtmTrack;

typedef struct FtmLfoState {
    FtmLfoTarget target;
    bool loop_modulation;
    const int8_t* shape_table;
    int shape_table_position;
    uint16_t modulation_speed;
    uint16_t modulation_depth;
    int16_t modulation_value;
} FtmLfoState;

typedef struct FtmSelState {
    FtmSelScript* script;
    int script_position;
    uint16_t wait_counter;
    uint16_t loop_counter;
    int voice_info_index; // index into voices[] for the target voice
    uint16_t new_pitch_goto;
    uint16_t new_volume_goto;
    uint16_t new_sample_goto;
    uint16_t release_goto;
    uint16_t portamento_goto;
    uint16_t vol_down_goto;
    uint16_t interrupt_line;
} FtmSelState;

typedef struct FtmVoiceInfo {
    int channel_number;

    int sample_index; // index into samples[]
    int16_t sample_number;
    int8_t* sample_data;
    uint32_t sample_data_size; // bytes allocated at sample_data; playback never reads past it
    uint32_t sample_start_offset;
    uint32_t sample_calc_offset;
    uint16_t sample_oneshot_length;
    uint32_t sample_loop_start;
    uint16_t sample_loop_length;
    uint32_t sample_total_length;

    uint16_t volume;
    uint16_t note_index;
    bool retrig_sample;

    // For display: the track note (1-34) last started on the voice and the sample it plays; 0 / -1 = none
    uint8_t shown_note;
    int shown_sample;

    int detune_index;

    FtmTrack* track;
    uint32_t track_position;
    int16_t rows_left_to_skip;

    int16_t portamento_ticks;
    uint32_t portamento_note;
    uint16_t portamento_end_note;

    uint32_t vol_down_volume;
    uint16_t vol_down_speed;

    FtmSelState sel_state;
    FtmLfoState lfos[4];
} FtmVoiceInfo;

typedef struct FtmPatternLoopInfo {
    uint32_t track_position;
    int channel; // whose track track_position indexes (edits renumber lines)
    uint16_t loop_start_pos;
    int16_t loop_count;
    int16_t orig_loop_count;
} FtmPatternLoopInfo;

typedef struct FtmGlobalInfo {
    uint16_t start_row;
    uint16_t end_row;
    uint8_t global_volume;
    uint16_t speed;
    FtmPatternLoopInfo pat_loop_stack[FTM_MAX_PAT_LOOPS];
    int pat_loop_top;
    uint16_t pat_loop_stop_row;
    uint16_t pat_loop_start_row;
    bool do_pat_loop;
    uint16_t speed_counter;
    uint16_t current_row;
    int16_t detune_values[4];
    uint16_t shown_row; // the row last handled; current_row already points past it
} FtmGlobalInfo;

// Paula registers of one channel, written during a tick and handed to the voices at its end.
typedef struct FtmChannel {
    bool muted;
    bool play_pending; // a new sample starts at the end of this tick
    int8_t* sample_data;
    uint32_t sample_data_size;
    uint32_t sample_offset;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint32_t period; // frequency as frequency, not period
    uint16_t volume;
    uint16_t panning; // 0=left, 1=right
} FtmChannel;

struct FtmModule {
    NuadeVoices* nuade_voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    uint16_t num_measures;
    uint8_t rows_per_measure;
    uint16_t start_cia;
    uint8_t start_speed;
    uint8_t channel_mute_status;
    uint8_t flag;
    uint8_t num_samples;
    char title[33];
    char artist[33];
    char sample_names[FTM_MAX_SAMPLES][30];
    bool sample_stored[FTM_MAX_SAMPLES]; // the sample has a header and data in the file

    FtmSample samples[FTM_MAX_SAMPLES];
    FtmSelScript* sel_scripts[FTM_MAX_SELS];
    FtmTrack tracks[8];

    int channel_mapping[8];
    int active_channel_count;

    FtmGlobalInfo playing_info;
    FtmVoiceInfo voices[8];
    FtmChannel channels[8];

    uint32_t loop_count;
    uint16_t cia_value;

    uint8_t visited[FTM_MAX_VISITED / 8];
};

// True when the data carries the FTM mark and a consistent header.
bool ftm_probe(const uint8_t* data, size_t size);

FtmModule* ftm_create(const uint8_t* data, size_t size);
void ftm_destroy(FtmModule* module);

int ftm_subsong_count(const FtmModule* module);
// (Re)start from the first row; the module plays through `voices`.
bool ftm_start(FtmModule* module, int subsong, NuadeVoices* voices);

int ftm_channel_count(const FtmModule* module);
void ftm_set_channel_mask(FtmModule* module, uint32_t mask);

// Runs one player tick and sets the voices. True when the song passed its loop point.
bool ftm_tick(FtmModule* module);
// CIA ticks until the next ftm_tick.
uint32_t ftm_tick_length(const FtmModule* module);

// Times the song ran back into a position it had already played.
uint32_t ftm_loop_count(const FtmModule* module);

const char* ftm_title(const FtmModule* module);
const char* ftm_artist(const FtmModule* module);

FtmPosition ftm_position(const FtmModule* module);
// The track (0-7) that voice `voice` plays, -1 if there is no such voice.
int ftm_voice_track(const FtmModule* module, int voice);
FtmVoiceState ftm_voice_state(const FtmModule* module, int voice);

// Edit API
int ftm_get_instrument_count(const FtmModule* module);
const char* ftm_get_instrument_name(const FtmModule* module, int inst);
int ftm_get_num_measures(const FtmModule* module);
int ftm_get_rows_per_measure(const FtmModule* module);

// Grid cells: `channel` is the track (0-7), `row` the song row (measure *
// rows per measure + row), as PlayFTM places a track's events. A cell is the
// event on that row: note (0 none, 1-34, 35 release), effect (FtmTrackEffect
// 0-14: 0 instrument only, 1-10 volume step + instrument, 11 SEL, 12 pitch
// bend, 13 volume down, 14 pattern loop) and its 6-bit argument.
void ftm_get_cell(const FtmModule* module, int channel, int row, uint8_t* note, uint8_t* effect, uint16_t* effect_arg);
// Writes, inserts or (all zero) removes the event on that row, re-spacing the
// track. Returns 1 when done, 0 for an invalid cell or a row that holds a
// pattern loop point (not a grid cell).
int ftm_set_cell(FtmModule* module, int channel, int row, uint8_t note, uint8_t effect, uint16_t effect_arg);

float ftm_get_instrument_param(const FtmModule* module, int inst, const char* param);
void ftm_set_instrument_param(FtmModule* module, int inst, const char* param, float value);

size_t ftm_export(const FtmModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
FtmModule* ftm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
