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

// The player's tick rate: one is1_tick per VBlank.
#define IS1_TICK_HZ 50

#define IS1_ARPEGGIO_TABLE_LENGTH 16

typedef struct Is1Module Is1Module;

typedef struct Is1Position {
    int position; // the position and row the last tick that started a row played
    int row;
    int rows; // rows in that position
    int speed;
} Is1Position;

typedef struct Is1VoiceState {
    int note; // the note last started on the voice (after the position's transpose; period table index), 0 if none
    int instrument; // 1.., 0 if none
} Is1VoiceState;

// A track line as written in the module; 0 in a field the line leaves empty.
typedef struct Is1Cell {
    uint8_t note; // period table index, 0x7f stops the voice
    uint8_t instrument;
    uint8_t arpeggio;
    uint8_t effect;
    uint8_t effect_arg;
} Is1Cell;

typedef enum Is1Effect {
    IS1_EFFECT_NONE = 0x0,
    IS1_EFFECT_SET_SLIDE_SPEED = 0x1,
    IS1_EFFECT_RESTART_ADSR = 0x2,
    IS1_EFFECT_RESTART_EGC = 0x3,
    IS1_EFFECT_SET_SLIDE_INCREMENT = 0x4,
    IS1_EFFECT_SET_VIBRATO_DELAY = 0x5,
    IS1_EFFECT_SET_VIBRATO_POSITION = 0x6,
    IS1_EFFECT_SET_VOLUME = 0x7,
    IS1_EFFECT_SKIP_NT = 0x8,
    IS1_EFFECT_SKIP_ST = 0x9,
    IS1_EFFECT_SET_TRACK_LEN = 0xA,
    IS1_EFFECT_SKIP_PORTAMENTO = 0xB,
    IS1_EFFECT_EFF_C = 0xC,
    IS1_EFFECT_EFF_D = 0xD,
    IS1_EFFECT_SET_FILTER = 0xE,
    IS1_EFFECT_SET_SPEED = 0xF
} Is1Effect;

typedef enum Is1EgcMode { IS1_EGC_OFF = 0, IS1_EGC_ONES = 1, IS1_EGC_REPEAT = 2 } Is1EgcMode;

typedef struct Is1SongInfo {
    uint8_t start_speed;
    uint8_t rows_per_track;
    uint16_t first_position;
    uint16_t last_position;
    uint16_t restart_position;
} Is1SongInfo;

typedef struct Is1SinglePositionInfo {
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;
} Is1SinglePositionInfo;

typedef struct Is1TrackLine {
    uint8_t note;
    uint8_t instrument;
    uint8_t arpeggio;
    Is1Effect effect;
    uint8_t effect_arg;
} Is1TrackLine;

typedef struct Is1Sample {
    int8_t* sample_addr;
    uint32_t length;
} Is1Sample;

typedef struct Is1Instrument {
    uint8_t waveform_number;
    bool synthesis_enabled;
    uint16_t waveform_length;
    uint16_t repeat_length;
    uint8_t volume;
    int8_t portamento_speed;
    bool adsr_enabled;
    uint8_t adsr_table_number;
    uint16_t adsr_table_length;
    bool portamento_enabled;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint8_t vibrato_level;
    uint8_t egc_offset;
    Is1EgcMode egc_mode;
    uint8_t egc_table_number;
    uint16_t egc_table_length;
} Is1Instrument;

typedef struct Is1VoiceInfo {
    // Position information
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;

    // Track row information
    uint8_t note;
    uint8_t instrument;
    uint8_t arpeggio;
    Is1Effect effect;
    uint8_t effect_arg;

    uint8_t use_buffer;
    int8_t synth_sample1[256];
    int8_t synth_sample2[256];

    uint8_t transposed_note;
    uint8_t previous_transposed_note;

    uint8_t transposed_instrument;

    uint8_t current_volume;

    int8_t slide_speed;
    int16_t slide_increment;

    bool portamento_enabled;
    int8_t portamento_speed;
    int16_t portamento_speed_counter;

    uint8_t vibrato_delay;
    uint8_t vibrato_position;

    bool adsr_enabled;
    uint16_t adsr_position;

    Is1EgcMode egc_mode;
    uint16_t egc_position;
} Is1VoiceInfo;

// A voice's Paula registers: written during a tick, handed to the voice at its end
typedef struct Is1Channel {
    bool active;       // the voice plays (read back from the voice at the start of a tick)
    bool play_pending; // a sample starts at the end of this tick
    bool swap_pending; // the sample data was replaced; the voice takes it over at its loop end
    int8_t* sample_data;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t volume; // 0-64 (Amiga range)
    uint32_t period;
} Is1Channel;

struct Is1Module {
    char title[29];

    Is1SongInfo* sub_songs;
    uint8_t num_sub_songs;

    Is1SinglePositionInfo** positions; // [totalPositions][4]
    uint16_t total_positions;

    Is1TrackLine* track_lines;
    uint32_t total_track_rows;

    Is1Sample* samples;
    uint8_t num_samples;

    int8_t** waveforms;
    uint8_t num_waveforms;

    Is1Instrument* instruments;
    uint8_t num_instruments;

    uint8_t* eg_tables;
    uint8_t num_eg_tables;

    uint8_t* adsr_tables;
    uint8_t num_adsr_tables;

    uint8_t arpeggio_tables[16 * IS1_ARPEGGIO_TABLE_LENGTH];

    int8_t vibrato_table[256];

    Is1SongInfo* current_song_info;

    // Playing info
    uint8_t speed_counter;
    uint8_t current_speed;
    uint16_t song_position;
    uint8_t row_position;
    uint8_t rows_per_track;

    // The position and row the last get_next_row played (song_position and row_position are already past them), and
    // the rows its position has when that row is done.
    uint16_t shown_position;
    uint8_t shown_row;
    uint8_t shown_rows;

    Is1VoiceInfo voices[4];
    Is1Channel channels[4];

    uint32_t loop_count; // times the song ran back into a position it had already played

    // Position visited tracking
    uint8_t visited[256];

    NuadeVoices* nuade_voices;
};

// True when the data carries an InStereo! 1.0 module mark.
bool is1_probe(const uint8_t* data, size_t size);

Is1Module* is1_create(const uint8_t* data, size_t size);
void is1_destroy(Is1Module* module);

int is1_subsong_count(const Is1Module* module);

// (Re)start a subsong from its first position. The voices must outlive the playback.
bool is1_start(Is1Module* module, int subsong, NuadeVoices* voices);

// Runs one player tick and hands the result to the voices. True when the song ran back into a position it had played.
bool is1_tick(Is1Module* module);

const char* is1_title(const Is1Module* module);

Is1Position is1_position(const Is1Module* module);
int is1_position_count(const Is1Module* module);
// Rows per position of the subsong until a track length effect changes it.
int is1_default_rows(const Is1Module* module);
Is1VoiceState is1_voice_state(const Is1Module* module, int voice);
// The line of `channel` at `row` of `position`; all 0 outside the song.
Is1Cell is1_pattern_cell(const Is1Module* module, int position, int channel, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
Is1Module* is1_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
