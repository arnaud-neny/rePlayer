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

// The player's tick rate: one sm_tick per VBlank.
#define SM_TICK_HZ 50

// Display
#define SM_ROWS 16 // rows in every track

// Position visited tracking for end detection
#define SM_MAX_POSITIONS 256

typedef struct SmModule SmModule;

typedef struct SmPosition {
    int step;
    int row;   // the row the last row tick played
    int speed; // ticks per row
} SmPosition;

typedef struct SmVoiceState {
    int note;       // the note last started on the voice as the track has it, 0 if none
    int instrument; // 1-15, 0 if none
} SmVoiceState;

// A track entry as written in the module; 0 in a field the entry leaves empty.
typedef struct SmCell {
    uint8_t note;
    uint8_t instrument;
    uint8_t effect;
    uint8_t effect_arg;
} SmCell;

typedef enum SmModuleType { SM_TYPE_UNKNOWN = 0, SM_TYPE_SOUNDMON_11, SM_TYPE_SOUNDMON_22 } SmModuleType;

typedef struct SmSampleInstrument {
    char name[25];
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
    uint16_t volume;
    int8_t* adr;
} SmSampleInstrument;

typedef struct SmSynthInstrument {
    uint16_t volume;
    uint8_t wave_table;
    uint16_t wave_length;
    uint8_t adsr_control;
    uint8_t adsr_table;
    uint16_t adsr_length;
    uint8_t adsr_speed;
    uint8_t lfo_control;
    uint8_t lfo_table;
    uint8_t lfo_depth;
    uint16_t lfo_length;
    uint8_t lfo_delay;
    uint8_t lfo_speed;
    uint8_t eg_control;
    uint8_t eg_table;
    uint8_t eg_start; // 1.1 only: initial EG position, 0 when absent
    uint16_t eg_length;
    uint8_t eg_delay;
    uint8_t eg_speed;
    uint8_t fx_control;
    uint8_t fx_speed;
    uint8_t fx_delay;
    uint8_t mod_control;
    uint8_t mod_table;
    uint8_t mod_speed;
    uint8_t mod_delay;
    uint16_t mod_length;
} SmSynthInstrument;

typedef struct SmInstrument {
    bool is_synth;
    SmSampleInstrument sample;
    SmSynthInstrument synth;
} SmInstrument;

typedef struct SmStep {
    uint16_t track_number;
    int8_t sound_transpose;
    int8_t transpose;
} SmStep;

typedef struct SmTrack {
    int8_t note;
    uint8_t instrument;
    uint8_t optional; // SmOptional value
    uint8_t optional_data;
} SmTrack;

typedef struct SmBpCurrent {
    bool restart;
    bool retrigger; // the voice restarts its sample from the start, not just its note state
    bool use_default_volume;
    bool synth_mode;
    int synth_offset;
    uint16_t period;
    uint8_t volume;
    uint8_t instrument;
    uint8_t note;
    uint8_t arp_value;
    int8_t auto_slide;
    uint8_t auto_arp;
    uint16_t eg_ptr;
    uint16_t lfo_ptr;
    uint16_t adsr_ptr;
    uint16_t mod_ptr;
    uint8_t eg_count;
    uint8_t lfo_count;
    uint8_t adsr_count;
    uint8_t mod_count;
    uint8_t fx_count;
    uint8_t old_eg_value;
    uint8_t eg_control;
    uint8_t lfo_control;
    uint8_t adsr_control;
    uint8_t mod_control;
    uint8_t fx_control;
    int8_t vibrato;
} SmBpCurrent;

typedef struct SmGlobalPlayingInfo {
    uint16_t bp_step;
    uint16_t shown_step; // step and row of the row the last bp_next played
    uint8_t shown_row;
    uint8_t vib_index;
    uint8_t arp_count;
    uint8_t bp_count;
    uint8_t bp_delay;
    int8_t st;
    int8_t tr;
    uint8_t bp_pat_count;
    uint8_t bp_rep_count;
    bool rep_fresh; // no repeat count has been set or consumed yet
    uint8_t new_pos;
    bool pos_flag;
    bool step_changed; // SM 1.1: a jump moved the step during this row
    bool first_repeat;
    int8_t* wave_tables;
    int wave_tables_size;
    int8_t synth_buffer[4][32];
} SmGlobalPlayingInfo;

// Mixer channel state (maps IChannel calls)
typedef struct SmChannel {
    const int8_t* sample_data;
    uint32_t sample_length;
    uint32_t sample_offset;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;   // 0-256
    bool active;       // the voice is playing (read back from the mixer at the start of a tick)
    bool play_pending; // a new sample starts at the end of this tick
    bool muted;
    int16_t sample_number;
} SmChannel;

struct SmModule {
    // Module data
    SmModuleType module_type;
    char module_name[27];
    uint8_t wave_num;
    uint16_t step_num;
    uint16_t track_num;

    SmInstrument instruments[15];
    SmStep* steps[4];    // steps[channel][step_index]
    SmTrack** tracks;    // tracks[track_index][row_index (0-15)]
    int8_t* wave_tables; // raw wave table data (waveNum * 64)

    // Playing state
    SmGlobalPlayingInfo playing_info;
    SmBpCurrent bp_current[4];
    bool end_reached;
    uint32_t loop_count; // times the song ran back into a position it had already played
    uint8_t visited[SM_MAX_POSITIONS];
    bool amiga_filter;

    // Paula registers: written by the player during a tick, handed to the voices at its end
    SmChannel channels[4];
    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
};

// True when the data carries a SoundMon module mark.
bool sm_probe(const uint8_t* data, size_t size);

SmModule* sm_create(const uint8_t* data, size_t size);
void sm_destroy(SmModule* module);

int sm_subsong_count(const SmModule* module);
// (Re)start from the first position; the player drives `voices` from then on.
bool sm_start(SmModule* module, int subsong, NuadeVoices* voices);

int sm_channel_count(const SmModule* module);
void sm_set_channel_mask(SmModule* module, uint32_t mask);

// Run one player tick and set the voices. Returns true when the song passed its loop point during the tick.
bool sm_tick(SmModule* module);

// Times the song ran back into a position it had already played.
uint32_t sm_loop_count(const SmModule* module);

const char* sm_title(const SmModule* module);
const char* sm_format_name(const SmModule* module);

int sm_step_count(const SmModule* module);
SmPosition sm_position(const SmModule* module);
SmVoiceState sm_voice_state(const SmModule* module, int voice);
// The entry of `channel` at `row` of `step`; all 0 outside the song.
SmCell sm_pattern_cell(const SmModule* module, int step, int channel, int row);

// Edit API
int sm_get_instrument_count(const SmModule* module);
int sm_get_num_tracks(const SmModule* module);
void sm_get_cell(const SmModule* module, int track_idx, int row, uint8_t* note, uint8_t* instrument, uint8_t* effect,
                 uint8_t* effect_arg);
void sm_set_cell(SmModule* module, int track_idx, int row, uint8_t note, uint8_t instrument, uint8_t effect,
                 uint8_t effect_arg);
const char* sm_get_instrument_name(const SmModule* module, int inst);
float sm_get_instrument_param(const SmModule* module, int inst, const char* param);
void sm_set_instrument_param(SmModule* module, int inst, const char* param, float value);
size_t sm_export(const SmModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
SmModule* sm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
