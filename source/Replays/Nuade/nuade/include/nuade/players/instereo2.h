// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)

#pragma once

#include <nuade/nuade.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// The unit of is2_tick_length.
#define IS2_TIMEBASE_HZ 709379

typedef struct Is2Module Is2Module;

typedef struct Is2Position {
    int position; // the position and row the last tick that started a row played
    int row;
    int rows; // rows in that position
    int speed;
} Is2Position;

typedef struct Is2VoiceState {
    int note; // the note last started on the voice (after the position's transpose; period table index), 0 if none
    int instrument; // 1.., 0 if none
} Is2VoiceState;

// A track line as written in the module; 0 in a field the line leaves empty.
typedef struct Is2Cell {
    uint8_t note; // period table index, 0x7f stops the voice, 0x80 does nothing
    uint8_t instrument;
    bool disable_sound_transpose;
    bool disable_note_transpose;
    uint8_t arpeggio;
    uint8_t effect;
    uint8_t effect_arg;
} Is2Cell;

typedef enum Is2Effect {
    IS2_EFFECT_ARPEGGIO = 0x0,
    IS2_EFFECT_SET_SLIDE_SPEED = 0x1,
    IS2_EFFECT_RESTART_ADSR = 0x2,
    IS2_EFFECT_SET_VIBRATO = 0x4,
    IS2_EFFECT_SET_PORTAMENTO = 0x7,
    IS2_EFFECT_SKIP_PORTAMENTO = 0x8,
    IS2_EFFECT_SET_TRACK_LEN = 0x9,
    IS2_EFFECT_SET_VOLUME_INCREMENT = 0xA,
    IS2_EFFECT_POSITION_JUMP = 0xB,
    IS2_EFFECT_SET_VOLUME = 0xC,
    IS2_EFFECT_TRACK_BREAK = 0xD,
    IS2_EFFECT_SET_FILTER = 0xE,
    IS2_EFFECT_SET_SPEED = 0xF
} Is2Effect;

typedef enum Is2EnvelopeGeneratorMode {
    IS2_EG_DISABLED = 0,
    IS2_EG_CALC = 1,
    IS2_EG_FREE = 2
} Is2EnvelopeGeneratorMode;

typedef enum Is2VoicePlayingMode { IS2_MODE_SAMPLE = 0, IS2_MODE_SYNTH = 1 } Is2VoicePlayingMode;

typedef struct Is2Arpeggio {
    uint8_t length;
    uint8_t repeat;
    int8_t values[14];
} Is2Arpeggio;

typedef struct Is2TrackLine {
    uint8_t note;
    uint8_t instrument;
    bool disable_sound_transpose;
    bool disable_note_transpose;
    uint8_t arpeggio;
    Is2Effect effect;
    uint8_t effect_arg;
} Is2TrackLine;

typedef struct Is2SinglePositionInfo {
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;
} Is2SinglePositionInfo;

typedef struct Is2SongInfo {
    uint8_t start_speed;
    uint8_t rows_per_track;
    uint16_t first_position;
    uint16_t last_position;
    uint16_t restart_position;
    uint16_t tempo;
} Is2SongInfo;

typedef struct Is2Sample {
    uint16_t one_shot_length;
    uint16_t repeat_length;
    int8_t sample_number;
    uint8_t volume;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint8_t vibrato_level;
    uint8_t portamento_speed;
} Is2Sample;

typedef struct Is2Instrument {
    // The player divides the portamento by byte 9 of the instrument record (a character of the name), not by
    // portamento_speed, which only loads the step counter.
    uint8_t portamento_divisor;
    uint16_t waveform_length;
    uint8_t volume;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint8_t vibrato_level;
    uint8_t portamento_speed;
    uint8_t adsr_length;
    uint8_t adsr_repeat;
    uint8_t sustain_point;
    uint8_t sustain_speed;
    uint8_t amf_length;
    uint8_t amf_repeat;
    Is2EnvelopeGeneratorMode eg_mode;
    uint8_t start_len;
    uint8_t stop_rep;
    uint8_t speed_up;
    uint8_t speed_down;
    uint8_t adsr_table[128];
    int8_t lfo_table[128];
    Is2Arpeggio arpeggios[3];
    uint8_t eg_table[128];
    int8_t waveform1[256];
    int8_t waveform2[256];
} Is2Instrument;

typedef struct Is2VoiceInfo {
    // Position information
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;

    // Track row information
    uint8_t note;
    uint8_t instrument;
    bool disable_sound_transpose;
    bool disable_note_transpose;
    uint8_t arpeggio;
    Is2Effect effect;
    uint8_t effect_arg;

    uint8_t transposed_note;
    uint8_t previous_transposed_note;

    uint8_t transposed_instrument;
    Is2VoicePlayingMode playing_mode;

    uint8_t current_volume;

    uint16_t arpeggio_position;
    bool arpeggio_effect_nibble;

    int8_t slide_speed;
    int16_t slide_value;

    uint16_t portamento_speed_counter;
    uint16_t portamento_speed;

    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint8_t vibrato_level;
    uint16_t vibrato_position;

    uint16_t adsr_position;
    uint16_t sustain_counter;

    int8_t eg_duration;
    uint16_t eg_position;

    uint16_t lfo_position;
} Is2VoiceInfo;

typedef struct Is2GlobalPlayingInfo {
    uint8_t speed_counter;
    uint8_t current_speed;
    int16_t song_position;
    uint8_t row_position;
    uint8_t rows_per_track;

    // The position and row the last get_next_row played, and the rows its position has once the row is done (a track
    // length effect changes them). song_position and row_position can already point elsewhere after a jump effect.
    uint16_t shown_position;
    uint8_t shown_row;
    uint8_t shown_rows;
} Is2GlobalPlayingInfo;

// A channel's Paula registers. Sample positions are bytes into sample_data.
typedef struct Is2Channel {
    int8_t* sample_data;
    uint32_t data_size; // bytes readable at sample_data
    uint32_t sample_length;
    uint32_t sample_offset;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume; // 0..64 for Amiga, 0..256 for synth
    bool active;
    bool use_volume_256; // true if SetVolume(0-256), false if SetAmigaVolume(0-64)

    // What the tick wrote since the last sync.
    bool play_pending;        // a note started
    bool latch_pending;       // a new window was latched while the voice plays (the envelope generator)
    NuadeSample first_window; // the window the note started with, when a latch followed in the same tick
} Is2Channel;

struct Is2Module {
    NuadeVoices* paula;

    Is2SongInfo* sub_songs;
    int num_sub_songs;

    Is2SinglePositionInfo** positions; // positions[i] is array of 4
    int num_positions;

    Is2TrackLine* track_lines;
    int num_track_lines;

    Is2Sample* samples;
    int num_samples;

    int8_t** sample_data;
    uint32_t* sample_data_size;
    int num_sample_data;

    Is2Instrument* instruments;
    int num_instruments;

    Is2SongInfo* current_song_info;

    Is2GlobalPlayingInfo playing_info;
    Is2VoiceInfo voices[4];
    Is2Channel channels[4];

    uint32_t loop_count;
    uint16_t tempo; // player ticks per second

    FILE* dump_state; // per-tick voice state, for A/B against the original; null when off
    uint32_t tick_number;

    // Position visit tracking
    uint8_t visited[1024];
};

// True when the data carries an InStereo! 2.0 module mark.
bool is2_probe(const uint8_t* data, size_t size);

Is2Module* is2_create(const uint8_t* data, size_t size);
void is2_destroy(Is2Module* module);

int is2_subsong_count(const Is2Module* module);
// (Re)start a subsong from its first position; the player drives `voices` from then on.
bool is2_start(Is2Module* module, int subsong, NuadeVoices* voices);

// Run one player tick and set the voices. Returns true when the song passed its loop point during the tick.
bool is2_tick(Is2Module* module);
// Time from this tick to the next, in IS2_TIMEBASE_HZ units (the subsong's tempo is a rate in Hz).
uint32_t is2_tick_length(const Is2Module* module);

// Write one line of voice state per voice and tick to the file at `path` ("-" is stderr, "" turns it off).
bool is2_set_dump_file(Is2Module* module, const char* path);

// Times the song ran back into a position it had already played.
uint32_t is2_loop_count(const Is2Module* module);

Is2Position is2_position(const Is2Module* module);
int is2_position_count(const Is2Module* module);
// Rows per position of the subsong until a track length effect changes it.
int is2_default_rows(const Is2Module* module);
Is2VoiceState is2_voice_state(const Is2Module* module, int voice);
// The line of `channel` at `row` of `position`; all 0 outside the song.
Is2Cell is2_pattern_cell(const Is2Module* module, int position, int channel, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
Is2Module* is2_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
