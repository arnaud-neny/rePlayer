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

// Player ticks per second (VBlank).
#define SYN_TICK_HZ 50

// Synth effects index waveforms with sums of byte parameters, so waveform buffers are padded to cover them.
#define SYN_WAVE_ALLOC 1024
#define SYN_ARPEGGIO_TABLE_LENGTH 16
#define SYN_MAX_VISITED 4096

typedef struct SynModule SynModule;

typedef struct SynPosition {
    int position; // the position and row the last tick that started a row played
    int row;
    int rows; // rows in that position
    int speed;
} SynPosition;

typedef struct SynVoiceState {
    int note; // the note last started on the voice (after the position's transpose; period table index), 0 if none
    int instrument; // 1.., 0 if none
} SynVoiceState;

typedef enum SynEffect {
    SYN_EFFECT_NONE = 0x0,
    SYN_EFFECT_SLIDE = 0x1,
    SYN_EFFECT_RESTART_ADSR = 0x2,
    SYN_EFFECT_RESTART_EGC = 0x3,
    SYN_EFFECT_SET_TRACK_LEN = 0x4,
    SYN_EFFECT_SKIP_STNT = 0x5,
    SYN_EFFECT_SYNC_MARK = 0x6,
    SYN_EFFECT_SET_FILTER = 0x7,
    SYN_EFFECT_SET_SPEED = 0x8,
    SYN_EFFECT_ENABLE_FX = 0x9,
    SYN_EFFECT_CHANGE_FX = 0xA,
    SYN_EFFECT_CHANGE_ARG1 = 0xB,
    SYN_EFFECT_CHANGE_ARG2 = 0xC,
    SYN_EFFECT_CHANGE_ARG3 = 0xD,
    SYN_EFFECT_EGC_OFF = 0xE,
    SYN_EFFECT_SET_VOLUME = 0xF
} SynEffect;

typedef enum SynSynthEffect {
    SYN_SYNTH_NONE = 0,
    SYN_SYNTH_ROTATE1 = 1,
    SYN_SYNTH_ROTATE2 = 2,
    SYN_SYNTH_ALIEN = 3,
    SYN_SYNTH_NEGATOR = 4,
    SYN_SYNTH_POLYNEG = 5,
    SYN_SYNTH_SHAKER1 = 6,
    SYN_SYNTH_SHAKER2 = 7,
    SYN_SYNTH_AMF_LFO = 8,
    SYN_SYNTH_LASER = 9,
    SYN_SYNTH_OCTFX1 = 10,
    SYN_SYNTH_OCTFX2 = 11,
    SYN_SYNTH_ALISING = 12,
    SYN_SYNTH_EGFX1 = 13,
    SYN_SYNTH_EGFX2 = 14,
    SYN_SYNTH_CHANGER = 15,
    SYN_SYNTH_FMDRUM = 16
} SynSynthEffect;

typedef enum SynEgcMode { SYN_EGC_OFF = 0, SYN_EGC_ONES = 1, SYN_EGC_REPEAT = 2 } SynEgcMode;

typedef enum SynTransposeMode {
    SYN_TRANSPOSE_ENABLED = 0,
    SYN_TRANSPOSE_SOUND_DISABLED = 1,
    SYN_TRANSPOSE_NOTE_DISABLED = 2,
    SYN_TRANSPOSE_ALL_DISABLED = 3
} SynTransposeMode;

typedef struct SynTrackLine {
    uint8_t note;
    uint8_t instrument;
    uint8_t arpeggio;
    SynEffect effect;
    uint8_t effect_arg;
} SynTrackLine;

typedef struct SynSinglePositionInfo {
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;
} SynSinglePositionInfo;

typedef struct SynSongInfo {
    uint8_t start_speed;
    uint8_t rows_per_track;
    uint16_t first_position;
    uint16_t last_position;
    uint16_t restart_position;
} SynSongInfo;

typedef struct SynSample {
    int8_t* sample_addr;
    uint32_t length;
} SynSample;

typedef struct SynInstrument {
    uint8_t waveform_number;
    bool synthesis_enabled;
    uint16_t waveform_length;
    uint16_t repeat_length;
    uint8_t volume;
    int8_t portamento_speed;
    bool adsr_enabled;
    uint8_t adsr_table_number;
    uint16_t adsr_table_length;
    uint8_t arpeggio_start;
    uint8_t arpeggio_length;
    uint8_t arpeggio_repeat_length;
    SynSynthEffect effect;
    uint8_t effect_arg1;
    uint8_t effect_arg2;
    uint8_t effect_arg3;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint8_t vibrato_level;
    uint8_t egc_offset;
    SynEgcMode egc_mode;
    uint8_t egc_table_number;
    uint16_t egc_table_length;
} SynInstrument;

typedef struct SynVoiceInfo {
    // Position information
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;

    // Track row information
    uint8_t note;
    uint8_t instrument;
    uint8_t arpeggio;
    SynEffect effect;
    uint8_t effect_arg;

    uint8_t use_buffer;
    int8_t synth_sample1[SYN_WAVE_ALLOC];
    int8_t synth_sample2[SYN_WAVE_ALLOC];

    uint8_t transposed_note;
    uint8_t previous_transposed_note;

    uint8_t transposed_instrument;

    uint8_t current_volume;
    uint8_t new_volume;

    uint8_t arpeggio_position;

    int8_t slide_speed;
    int16_t slide_increment;

    int8_t portamento_speed;
    int16_t portamento_speed_counter;

    uint8_t vibrato_delay;
    uint8_t vibrato_position;

    bool adsr_enabled;
    uint16_t adsr_position;

    bool egc_disabled;
    SynEgcMode egc_mode;
    uint16_t egc_position;

    bool synth_effect_disabled;
    SynSynthEffect synth_effect;
    uint8_t synth_effect_arg1;
    uint8_t synth_effect_arg2;
    uint8_t synth_effect_arg3;

    uint8_t synth_position;
    uint8_t slow_motion_counter;
} SynVoiceInfo;

typedef struct SynGlobalPlayingInfo {
    uint8_t sync_mark;
    uint8_t speed_counter;
    uint8_t current_speed;
    uint16_t song_position;
    uint8_t row_position;
    uint8_t rows_per_track;
    SynTransposeMode transpose_enable_status;

    // The position and row the last get_next_row played (song_position and row_position are already past them), and
    // the rows its position has once the row is done.
    uint16_t shown_position;
    uint8_t shown_row;
    uint8_t shown_rows;
} SynGlobalPlayingInfo;

// Per-channel registers, handed to the voice at the end of each tick
typedef struct SynChannel {
    bool active;
    bool play_pending; // a note started this tick: restart the voice
    bool swap_pending; // the buffer or loop changed this tick: switch at the loop end

    int8_t* sample_data;
    uint32_t sample_offset;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;

    uint16_t period;
    uint16_t volume;
} SynChannel;

struct SynModule {
    NuadeVoices* voices_out;
    char title[29];

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    int start_offset;

    SynSongInfo* sub_songs;
    int num_sub_songs;

    SynSinglePositionInfo** positions; // positions[pos][voice]
    int num_positions;

    SynTrackLine* track_lines;
    int num_track_lines;

    SynSample* samples;
    int num_samples;

    int8_t** waveforms;
    int num_waveforms;

    SynInstrument* instruments;
    int num_instruments;

    uint8_t* egc_tables;
    int num_egc_tables;

    uint8_t* adsr_tables;
    int num_adsr_tables;

    uint8_t arpeggio_tables[16 * SYN_ARPEGGIO_TABLE_LENGTH];

    int8_t vibrato_table[256];

    SynSongInfo* current_song_info;

    SynGlobalPlayingInfo playing_info;
    SynVoiceInfo voices[4];

    SynChannel channels[4];

    uint32_t loop_count; // times the song ran back into a position it had already played

    // visited positions for end detection
    uint8_t visited[SYN_MAX_VISITED / 8];
};

// True when the data carries a Synthesis module mark.
bool syn_probe(const uint8_t* data, size_t size);

SynModule* syn_create(const uint8_t* data, size_t size);
void syn_destroy(SynModule* module);

int syn_subsong_count(const SynModule* module);
// (Re)start a subsong from its first position; the module drives `voices` from then on.
bool syn_start(SynModule* module, int subsong, NuadeVoices* voices);

int syn_channel_count(const SynModule* module);

SynPosition syn_position(const SynModule* module);
// Rows per position of the subsong until a track length effect changes it.
int syn_default_rows(const SynModule* module);
SynVoiceState syn_voice_state(const SynModule* module, int voice);

// Run one player tick and set the voices. True when the song ran back into a position it had already played.
bool syn_tick(SynModule* module);

const char* syn_title(const SynModule* module);
const char* syn_format_name(const SynModule* module);

// Edit API
int syn_get_instrument_count(const SynModule* module);
int syn_get_num_track_lines(const SynModule* module);
int syn_get_num_positions(const SynModule* module);

void syn_get_cell(const SynModule* module, int idx, uint8_t* note, uint8_t* instrument, uint8_t* arpeggio,
                  uint8_t* effect, uint8_t* effect_arg);
void syn_set_cell(SynModule* module, int idx, uint8_t note, uint8_t instrument, uint8_t arpeggio, uint8_t effect,
                  uint8_t effect_arg);

void syn_get_position(const SynModule* module, int pos, int channel, uint16_t* start_track_row, int8_t* sound_transpose,
                      int8_t* note_transpose);
void syn_set_position(SynModule* module, int pos, int channel, uint16_t start_track_row, int8_t sound_transpose,
                      int8_t note_transpose);

float syn_get_instrument_param(const SynModule* module, int inst, const char* param);
void syn_set_instrument_param(SynModule* module, int inst, const char* param, float value);

size_t syn_export(const SynModule* module, uint8_t* out, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
SynModule* syn_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
