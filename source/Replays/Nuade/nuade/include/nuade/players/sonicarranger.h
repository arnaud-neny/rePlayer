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

#define SA_VOICES 4
#define SA_MAX_POSITIONS 4096

// The player's timebase: sa_tick_length gives the next tick's length in these units (the subsong's tempo in Hz).
// PAL CIA clock, so a tempo in Hz gives a whole timer count per tick
#define SA_TIMEBASE_HZ 709379

#define SA_WAVEFORM_SIZE 128

typedef struct SaModule SaModule;

// ── Display ───────────────────────────────────────────────────────────────

typedef struct SaPosition {
    bool valid; // false before the first row
    int position;
    int row;  // the row the last row tick played
    int rows; // rows in the tracks as they stand now (a track length effect changes them)
    int speed;
    int tick; // tick within the row
} SaPosition;

typedef struct SaVoiceState {
    int note;       // period table index (1-108) of the last note started on the voice, after transpose, 0 if none
    int instrument; // its instrument number (1-based), after transpose, 0 if none
} SaVoiceState;

// A track entry as written in the module; 0 in a field the entry leaves empty. Note 0x7f stops the voice, 0x80
// releases it.
typedef struct SaCell {
    uint8_t note;
    uint8_t instrument;
    uint8_t arpeggio;
    uint8_t effect;
    uint8_t effect_arg;
} SaCell;

typedef enum SaInstrumentType { SA_INSTRUMENT_SAMPLE = 0, SA_INSTRUMENT_SYNTH = 1 } SaInstrumentType;

typedef enum SaSynthesisEffect {
    SA_SYNTH_NONE = 0,
    SA_SYNTH_WAVE_NEGATOR = 1,
    SA_SYNTH_FREE_NEGATOR = 2,
    SA_SYNTH_ROTATE_VERTICAL = 3,
    SA_SYNTH_ROTATE_HORIZONTAL = 4,
    SA_SYNTH_ALIEN_VOICE = 5,
    SA_SYNTH_POLY_NEGATOR = 6,
    SA_SYNTH_SHACK_WAVE_1 = 7,
    SA_SYNTH_SHACK_WAVE_2 = 8,
    SA_SYNTH_METAMORPH = 9,
    SA_SYNTH_LASER = 10,
    SA_SYNTH_WAVE_ALIAS = 11,
    SA_SYNTH_NOISE_GENERATOR_1 = 12,
    SA_SYNTH_LOW_PASS_FILTER_1 = 13,
    SA_SYNTH_LOW_PASS_FILTER_2 = 14,
    SA_SYNTH_OSZILATOR = 15,
    SA_SYNTH_NOISE_GENERATOR_2 = 16,
    SA_SYNTH_FM_DRUM = 17
} SaSynthesisEffect;

typedef enum SaEffect {
    SA_EFFECT_ARPEGGIO = 0x0,
    SA_EFFECT_SET_SLIDE_SPEED = 0x1,
    SA_EFFECT_RESTART_ADSR = 0x2,
    SA_EFFECT_SET_VIBRATO = 0x4,
    SA_EFFECT_SYNC = 0x5,
    SA_EFFECT_SET_MASTER_VOLUME = 0x6,
    SA_EFFECT_SET_PORTAMENTO = 0x7,
    SA_EFFECT_SKIP_PORTAMENTO = 0x8,
    SA_EFFECT_SET_TRACK_LEN = 0x9,
    SA_EFFECT_VOLUME_SLIDE = 0xA,
    SA_EFFECT_POSITION_JUMP = 0xB,
    SA_EFFECT_SET_VOLUME = 0xC,
    SA_EFFECT_TRACK_BREAK = 0xD,
    SA_EFFECT_SET_FILTER = 0xE,
    SA_EFFECT_SET_SPEED = 0xF
} SaEffect;

typedef struct SaArpeggio {
    uint8_t length;
    uint8_t repeat;
    int8_t values[14];
} SaArpeggio;

typedef struct SaTrackLine {
    uint8_t note;
    uint8_t instrument;
    bool disable_sound_transpose;
    bool disable_note_transpose;
    uint8_t arpeggio;
    SaEffect effect;
    uint8_t effect_arg;
} SaTrackLine;

typedef struct SaSinglePositionInfo {
    uint16_t start_track_row;
    int8_t sound_transpose;
    int8_t note_transpose;
} SaSinglePositionInfo;

typedef struct SaSongInfo {
    uint16_t start_speed;
    uint16_t rows_per_track;
    uint16_t first_position;
    uint16_t last_position;
    uint16_t restart_position;
    uint16_t tempo;
} SaSongInfo;

typedef struct SaGlobalPlayingInfo {
    uint16_t master_volume;
    uint16_t speed_counter;
    uint16_t current_speed;
    int16_t song_position;
    uint16_t row_position;
    uint16_t rows_per_track;

    // The position and row the last row tick played; a jump or break effect on that row has already moved
    // song_position / row_position on. shown_position is -1 before the first row.
    int16_t shown_position;
    uint16_t shown_row;
} SaGlobalPlayingInfo;

typedef struct SaInstrument {
    char name[31];
    SaInstrumentType type;
    uint16_t waveform_number;
    uint16_t waveform_length;
    uint16_t repeat_length;
    uint16_t volume;
    int16_t fine_tuning;
    uint16_t portamento_speed;
    uint16_t vibrato_delay;
    uint16_t vibrato_speed;
    uint16_t vibrato_level;
    uint16_t amf_number;
    uint16_t amf_delay;
    uint16_t amf_length;
    uint16_t amf_repeat;
    uint16_t adsr_number;
    uint16_t adsr_delay;
    uint16_t adsr_length;
    uint16_t adsr_repeat;
    uint16_t sustain_point;
    uint16_t sustain_delay;
    SaSynthesisEffect effect;
    uint16_t effect_arg1;
    uint16_t effect_arg2;
    uint16_t effect_arg3;
    uint16_t effect_delay;
    SaArpeggio arpeggios[3];
} SaInstrument;

typedef struct SaVoiceInfo {
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
    SaEffect effect;
    uint8_t effect_arg;

    uint16_t transposed_note;
    uint16_t previous_transposed_note;

    SaInstrumentType instrument_type;
    SaInstrument* instrument_info;
    uint16_t transposed_instrument;

    uint16_t current_volume;
    int16_t volume_slide_speed;

    uint16_t vibrato_position;
    uint16_t vibrato_delay;
    uint16_t vibrato_speed;
    uint16_t vibrato_level;

    uint16_t portamento_speed;
    uint16_t portamento_period;

    uint16_t arpeggio_position;

    int16_t slide_speed;
    int16_t slide_value;

    uint16_t adsr_position;
    uint16_t adsr_delay_counter;
    uint16_t sustain_delay_counter;

    uint16_t amf_position;
    uint16_t amf_delay_counter;

    uint16_t synth_effect_position;
    uint16_t synth_effect_wave_position;
    uint16_t effect_delay_counter;

    uint8_t flag;

    uint8_t shown_note;       // period table index (1-108) of the last note started on the voice, 0 if none
    uint8_t shown_instrument; // its instrument number (1-based), 0 if none

    int8_t waveform_buffer[SA_WAVEFORM_SIZE];
} SaVoiceInfo;

// The Paula registers of one voice: written during a tick, handed to the voice at its end.
typedef struct SaChannel {
    const int8_t* sample_data;
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;
    bool active;
    bool play_pending; // a note started this tick
} SaChannel;

struct SaModule {
    // Module data
    SaSongInfo* sub_songs;
    int num_sub_songs;

    SaSinglePositionInfo** positions; // [num_positions][4]
    int num_positions;

    SaTrackLine* track_lines;
    int num_track_lines;

    SaInstrument* instruments;
    int num_instruments;

    int8_t** sample_data;
    uint32_t* sample_lengths;
    int num_samples;

    int8_t** waveform_data;
    int num_waveforms;

    uint8_t** adsr_tables;
    int num_adsr_tables;

    uint8_t** amf_tables;
    int num_amf_tables;

    // Current song info
    SaSongInfo* current_song_info;

    // Playing state
    SaGlobalPlayingInfo playing_info;
    SaVoiceInfo voices[SA_VOICES];
    SaChannel channels[SA_VOICES];

    // Song positions already played; a position played again is a loop.
    uint8_t visited[SA_MAX_POSITIONS / 8];
    uint32_t loop_count;

    NuadeVoices* mixer;

    // Random state
    uint32_t random_state;

    // Original file data for export patching
    uint8_t* original_data;
    size_t original_size;
};

// True when the data carries a Sonic Arranger module mark (the compressed variant is not supported).
bool sa_probe(const uint8_t* data, size_t size);

SaModule* sa_create(const uint8_t* data, size_t size);
void sa_destroy(SaModule* module);

int sa_subsong_count(const SaModule* module);
// (Re)start a subsong from its first position; the player drives `voices` from then on.
bool sa_start(SaModule* module, int subsong, NuadeVoices* voices);
uint32_t sa_tick_length(const SaModule* module);

// Run one player tick and set the voices. Returns true when the song passed its loop point during the tick.
bool sa_tick(SaModule* module);

SaPosition sa_position(const SaModule* module);
SaVoiceState sa_voice_state(const SaModule* module, int voice);
// Rows per track the subsong starts with.
int sa_track_rows(const SaModule* module);
// The entry of `channel` at `row` of `position`; all 0 outside the song.
SaCell sa_pattern_cell(const SaModule* module, int position, int channel, int row);

// ── Edit API ──────────────────────────────────────────────────────────────

// Pattern info
int sa_get_num_positions(const SaModule* module);
int sa_get_num_track_lines(const SaModule* module);
int sa_get_rows_per_track(const SaModule* module, int subsong);

// Pattern cell access (track_line_index is the flat index into track_lines[])
void sa_get_cell(const SaModule* module, int track_line_index, uint8_t* note, uint8_t* instrument, uint8_t* arpeggio,
                 uint8_t* effect, uint8_t* effect_arg);
void sa_set_cell(SaModule* module, int track_line_index, uint8_t note, uint8_t instrument, uint8_t arpeggio,
                 uint8_t effect, uint8_t effect_arg);

// Position info (4 channels per position)
void sa_get_position(const SaModule* module, int pos, int channel, uint16_t* start_track_row, int8_t* sound_transpose,
                     int8_t* note_transpose);
void sa_set_position(SaModule* module, int pos, int channel, uint16_t start_track_row, int8_t sound_transpose,
                     int8_t note_transpose);

// Instrument count and parameter access
int sa_get_instrument_count(const SaModule* module);
// Returns instrument parameter as float. Returns -1 if param not found.
// Param names: "type","waveformNumber","waveformLength","repeatLength","volume",
// "fineTuning","portamentoSpeed","vibratoDelay","vibratoSpeed","vibratoLevel",
// "amfNumber","amfDelay","amfLength","amfRepeat","adsrNumber","adsrDelay",
// "adsrLength","adsrRepeat","sustainPoint","sustainDelay","effect",
// "effectArg1","effectArg2","effectArg3","effectDelay"
float sa_get_instrument_param(const SaModule* module, int inst, const char* param);
void sa_set_instrument_param(SaModule* module, int inst, const char* param, float value);

// Instrument name
const char* sa_get_instrument_name(const SaModule* module, int inst);

// Export: serialize current state back to SOARV1.0 binary format.
// Returns bytes written, or 0 on failure. If out_buffer is NULL, returns required size.
size_t sa_export(const SaModule* module, uint8_t* out_buffer, size_t max_size);

// The player's state behind a song it opened; 0 when the song is another player's.
SaModule* sa_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
