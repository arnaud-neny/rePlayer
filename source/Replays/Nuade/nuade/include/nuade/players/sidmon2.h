// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
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

#define SD2_VOICES 4

// The player's tick rate: one sd2_tick per VBlank.
#define SD2_TICK_HZ 50

// The lowest notes sit this many octaves below C-0.
#define SD2_NOTE_OCTAVE_SHIFT (-2)

typedef struct Sd2Module Sd2Module;

// What the display shows: every voice plays the sequence entry of the shared position (one track each), on a shared
// row. The row is the one the last row tick played; the note and instrument are those of the last note started (the
// note with the entry's transpose, as played).
typedef struct Sd2VoiceState {
    int note; // semitones from C-0 + SD2_NOTE_OCTAVE_SHIFT octaves, -1 if none
    int instrument;
    int order;
    int row;
    int speed; // ticks per row
} Sd2VoiceState;

// One row of a voice's track as written (no transpose). Fields not present are -1.
typedef struct Sd2Cell {
    int note;
    int instrument;
    int effect;
    int param;
} Sd2Cell;

typedef enum Sd2EnvelopeState {
    Sd2EnvelopeState_Done = 0,
    Sd2EnvelopeState_Attack,
    Sd2EnvelopeState_Decay,
    Sd2EnvelopeState_Sustain,
    Sd2EnvelopeState_Release
} Sd2EnvelopeState;

typedef struct Sd2Instrument {
    uint8_t waveform_list_number;
    uint8_t waveform_list_length;
    uint8_t waveform_list_speed;
    uint8_t waveform_list_delay;
    uint8_t arpeggio_number;
    uint8_t arpeggio_length;
    uint8_t arpeggio_speed;
    uint8_t arpeggio_delay;
    uint8_t vibrato_number;
    uint8_t vibrato_length;
    uint8_t vibrato_speed;
    uint8_t vibrato_delay;
    int8_t pitch_bend_speed;
    uint8_t pitch_bend_delay;
    uint8_t attack_max;
    uint8_t attack_speed;
    uint8_t decay_min;
    uint8_t decay_speed;
    uint8_t sustain_time;
    uint8_t release_min;
    uint8_t release_speed;
} Sd2Instrument;

typedef struct Sd2Sample {
    uint32_t length;
    uint32_t loop_start;
    uint32_t loop_length;
    int8_t* sample_data;
} Sd2Sample;

typedef struct Sd2SampleNegateInfo {
    uint32_t start_offset;
    uint32_t end_offset;
    uint16_t loop_index;
    uint16_t status;
    int16_t speed;
    int32_t position;
    uint16_t index;
    int16_t do_negation;
} Sd2SampleNegateInfo;

typedef struct Sd2Sequence {
    uint8_t track_number;
    int8_t note_transpose;
    int8_t instrument_transpose;
} Sd2Sequence;

typedef struct Sd2Track {
    uint8_t* track_data;
    uint32_t length;
    uint32_t image_offset; // where the track starts in Sd2Module.image
    bool edited;           // playback reads track_data instead of the shared memory
} Sd2Track;

typedef struct Sd2VoiceInfo {
    Sd2Sequence* sequence_list;
    int8_t* sample_data;
    uint32_t sample_length;
    uint16_t sample_period;
    int16_t sample_volume;
    Sd2EnvelopeState envelope_state;
    uint16_t sustain_counter;
    Sd2Instrument* instrument;
    Sd2Instrument unmapped_instrument; // what the byte index read from behind the instrument table
    int8_t* loop_sample;
    uint32_t loop_offset;
    uint32_t loop_length;
    uint16_t original_note;

    uint16_t wave_list_delay;
    int16_t wave_list_offset;
    uint16_t arpeggio_delay;
    int16_t arpeggio_offset;
    uint16_t vibrato_delay;
    int16_t vibrato_offset;

    uint16_t current_note;
    uint16_t current_instrument;
    uint16_t current_effect;
    uint16_t current_effect_arg;

    uint16_t pitch_bend_counter;
    int16_t instrument_transpose;
    int16_t pitch_bend_value;
    uint16_t note_slide_note;
    int16_t note_slide_speed;
    int32_t track_position;
    uint8_t* current_track;
    uint32_t current_track_length;
    uint16_t empty_notes_counter;
    int16_t note_transpose;
    uint16_t current_sample;

    int shown_note;       // note index of the last note started (with the position's transpose), -1 if none
    int shown_instrument; // instrument it plays (1-based, with the position's transpose), -1 if none
} Sd2VoiceInfo;

// One Paula channel's registers: written during a tick, handed to the voices at its end.
typedef struct Sd2VoiceRender {
    NuadeSample head;   // started when play_pending
    NuadeSample queued; // takes over when the playing sample ends
    uint16_t volume;
    uint16_t period;
    bool active; // false after a mute and once a one-shot ran off its end
    bool play_pending;
    bool queue_pending;
} Sd2VoiceRender;

struct Sd2Module {
    // Module data
    uint8_t** waveform_info; // [list_count][16]
    int32_t waveform_count;
    int8_t** arpeggios; // [arp_count][16]
    int32_t arpeggio_count;
    int8_t** vibratoes; // [vib_count][16]
    int32_t vibrato_count;
    Sd2Sequence* sequences; // [4 * num_positions]
    int32_t num_positions;  // number of positions per channel
    Sd2Track* tracks;
    int32_t track_count;
    // The module as the player sees it in memory. It has no end marker for a track and keeps reading into
    // the next one when the pattern is longer than the track, and it indexes the instrument table with a
    // byte without a bound check, so both can run on into the data behind them.
    uint8_t* image;
    uint32_t image_size;
    uint32_t instruments_offset;
    Sd2Instrument* instruments;
    int32_t instrument_count;
    Sd2Sample* samples;
    int32_t sample_count;
    Sd2SampleNegateInfo* sample_negate_info;

    // Playback state
    int16_t current_position; // 0..255, -1 only while wrapping to 0
    uint8_t current_row;
    uint8_t pattern_length;
    uint8_t current_rast;
    uint8_t current_rast2;
    uint8_t speed;
    uint8_t start_speed;
    uint8_t number_of_positions; // max position index (numberOfPositions in C#, after decrement)
    int shown_position;          // position and row of the last row tick, the row being heard
    int shown_row;

    Sd2VoiceInfo voices[4];
    Sd2VoiceRender voice_render[4];

    NuadeVoices* nuade_voices;
};

// True when the data carries the SidMon II module mark.
bool sd2_probe(const uint8_t* data, size_t size);

Sd2Module* sd2_create(const uint8_t* data, size_t size);
void sd2_destroy(Sd2Module* module);

int sd2_subsong_count(const Sd2Module* module);
// (Re)start a subsong from its beginning; the player drives `voices` from then on.
bool sd2_start(Sd2Module* module, int subsong, NuadeVoices* voices);

int sd2_channel_count(const Sd2Module* module);

// One player tick. True when the song went past its last position and back to the start during it.
bool sd2_tick(Sd2Module* module);

// Track editing API
int sd2_get_num_tracks(const Sd2Module* module);
int sd2_get_track_length(const Sd2Module* module, int track_idx);
uint32_t sd2_get_cell(const Sd2Module* module, int track_idx, int row);
void sd2_set_cell(Sd2Module* module, int track_idx, int row, int note, int instrument, int effect, int param);

// Instrument preview API
int sd2_get_instrument_count(const Sd2Module* module);
void sd2_preview_note_on(Sd2Module* module, int instrument, int note, int velocity);
void sd2_preview_note_off(Sd2Module* module);

// Serialize module to binary (caller must free *out_data)
uint32_t sd2_serialize(const Sd2Module* module, uint8_t** out_data);

int sd2_order_count(const Sd2Module* module);
// Rows per position; effect 6 changes it.
int sd2_pattern_rows(const Sd2Module* module);
Sd2VoiceState sd2_voice_state(const Sd2Module* module, int voice);
// Row `row` of the track `voice` plays at position `order`, read the way the player does.
Sd2Cell sd2_pattern_cell(const Sd2Module* module, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
Sd2Module* sd2_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
