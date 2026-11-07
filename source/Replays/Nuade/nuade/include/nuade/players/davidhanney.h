// SPDX-License-Identifier: MIT
// David Hanney player (the music of "Tearaway Thomas", 1992), translated from the UADE eagleplayer "David Hanney
// player module V1.1" (Wanted Team). A module is a chunk list (sequence, info, patterns); the instruments are the
// sixteen built into the eagleplayer, so the eagleplayer holds the replayer and the sounds.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DH_VOICES 4

// The CIA clock: the timebase of DH_TICK_LENGTH.
#define DH_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / DH_TIMEBASE_HZ seconds.
#define DH_TICK_LENGTH 14187

// Note value of period table entry n is DH_NOTE_VALUE_BASE + n. The table starts three octaves below the one of C-1
// (entry 36, 856), so the Note column takes this octave shift to spell entry 36 as C-1 (see NuadeColumnDesc).
#define DH_NOTE_VALUE_BASE 12
#define DH_NOTE_OCTAVE_SHIFT (-3)

// Note bytes of a pattern cell: 1 stops the voice, 2 is meant to release it (the replayer damages its tables instead,
// see release_note in davidhanney.c), and from 3 up the note plays period table entry (note - DH_NOTE_TABLE_BIAS).
#define DH_NOTE_STOP 1
#define DH_NOTE_RELEASE 2
#define DH_NOTE_TABLE_BIAS 2

#define DH_INSTRUMENTS 16
#define DH_WAVE_BYTES 32 // the inline wave of a synthetic instrument: 16 words at most
#define DH_VIBRATO_STEPS 16
#define DH_ENVELOPE_STAGES 4
#define DH_CHORDS 14 // chords the table holds
#define DH_CHORD_STEPS 12

typedef struct DhPlayer DhPlayer;

typedef struct DhTickResult {
    bool looped; // the sequence wrapped (the eagleplayer's SongEnd)
} DhTickResult;

typedef struct DhPosition {
    int order; // position in the sequence
    int row;
    int rows;  // rows of the pattern
    int speed; // ticks per row
} DhPosition;

typedef struct DhVoiceState {
    int note;       // dh_note_value of the last note started, -1 if none
    int instrument; // 0-based built-in instrument of the last note started, -1 if none
} DhVoiceState;

// One pattern cell as stored.
typedef struct DhCell {
    uint8_t note; // 0 none, DH_NOTE_STOP, DH_NOTE_RELEASE, or period table entry + DH_NOTE_TABLE_BIAS
    uint8_t instrument;
    uint8_t effect; // 1 slide to the note, 2 envelope speed, 3 chord, 4 retrigger
    uint8_t param;
} DhCell;

typedef struct DhModule {
    uint8_t* data;
    uint32_t size;
} DhModule;

typedef enum DhVoiceMode {
    DhVoiceMode_Idle,
    DhVoiceMode_StartDma, // a one-shot wave was set up; the next tick switches the DMA on
    DhVoiceMode_Run,      // a synthetic instrument: every tick writes period and volume and keeps the DMA on
} DhVoiceMode;

typedef struct DhEnvelopeStage {
    uint8_t level; // 0..63 after the sanitising at the start; the volume reaches level * 4 of 255
    uint8_t rate;  // volume steps per envelope tick
} DhEnvelopeStage;

// An instrument is either one of three one-shot waves built into the eagleplayer (wave_kind 1..3; a bigger kind plays
// as 3), or, with wave_kind 0, a synthetic one: a short wave looped by Paula, with a volume envelope of four stages,
// a vibrato and a pulse width sweep that rewrites the wave while it plays.
typedef struct DhInstrument {
    uint16_t wave_kind;
    int8_t wave[DH_WAVE_BYTES];
    int8_t vibrato[DH_VIBRATO_STEPS]; // period offsets, scaled by vibrato_depth
    DhEnvelopeStage envelope[DH_ENVELOPE_STAGES];
    uint8_t vibrato_speed; // 0 switches the vibrato off
    uint8_t vibrato_delay; // ticks before it starts
    uint8_t vibrato_depth;
    uint8_t pwm_speed; // ticks per sweep step, 0 switches the sweep off
    uint8_t pwm_low;   // the sweep moves between these wave bytes
    uint8_t pwm_high;
    uint8_t length_words; // of the inline wave, 16 at most
    uint8_t vibrato_wrap; // the vibrato position restarts at 2 * this
    uint8_t pwm_pos;      // the wave byte the sweep writes next
    int8_t pwm_dir;       // +1 writes 0xC0 and moves up, -1 writes 0x3F and moves down
    uint8_t pwm_timer;
    uint8_t pwm_last_tick; // the player tick the sweep last ran in
} DhInstrument;

typedef struct DhVoice {
    NuadePaulaChannel paula;
    DhCell cell; // the event of the current row
    DhVoiceMode mode;
    DhInstrument* instrument;
    uint8_t retrigger_timer; // ticks until the note starts again, 0 when it does not
    uint8_t envelope_speed;
    uint8_t envelope_timer;
    uint8_t envelope_stage;
    uint16_t envelope_level; // 0..255
    uint8_t vibrato_pos;
    uint8_t vibrato_delay;
    uint8_t chord[DH_CHORD_STEPS];
    uint8_t chord_pos;
    uint16_t note_index; // period table index of the note
    uint16_t period;     // the note's period, or where a slide has got to
    uint16_t slide_target;
    uint16_t slide_speed; // 0 when no slide is active
    int note;
    int instrument_number;
} DhVoice;

struct DhPlayer {
    DhModule module;
    DhInstrument instruments[DH_INSTRUMENTS]; // the sweep rewrites their waves
    DhVoice voices[DH_VOICES];

    uint8_t chords[DH_CHORDS][DH_CHORD_STEPS]; // a copy: a release (note byte 2) writes into the replayer's table

    int64_t sequence_base; // data offset of SEQU
    int64_t info_base;     // data offset of INFO
    uint16_t sequence_length;
    uint16_t speed; // ticks per row - 1
    uint16_t position;
    uint8_t pattern; // the number of the pattern at `position`, signed in the tag arithmetic
    uint16_t row;
    uint16_t last_row; // rows of the pattern - 1
    uint16_t row_timer;
    uint8_t tick_counter; // counts the ticks of the song, wraps
    DhPosition shown;     // what the last row tick decoded
};

// The note value of a note byte, -1 for a byte that is not a table note (0, DH_NOTE_STOP, DH_NOTE_RELEASE).
int dh_note_value(uint8_t note);

// The eagleplayer's Check2: "DSNG", then the chunks "SEQU", "INFO" and "BLK\0", each skipped by its length, and the
// tag "BLK\1" after them.
bool dh_probe(const uint8_t* data, size_t size);

// 0 when the module is not accepted by dh_probe or memory runs out. The player keeps its own copy of the data.
DhPlayer* dh_create(const uint8_t* data, size_t size);
void dh_destroy(DhPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool dh_start(DhPlayer* player, NuadeVoices* voices);

DhTickResult dh_tick(DhPlayer* player, NuadeVoices* voices);

// The row decoded by the last row tick.
DhPosition dh_position(const DhPlayer* player);
int dh_order_count(const DhPlayer* player);

DhVoiceState dh_voice_state(const DhPlayer* player, int voice);

// `order` 0 .. dh_order_count - 1, `voice` 0..3, `row` 0 .. rows - 1 of the pattern at that order.
DhCell dh_pattern_cell(const DhPlayer* player, int order, int voice, int row);
// Rows of the pattern at `order`.
int dh_pattern_rows(const DhPlayer* player, int order);

// The player's state behind a song it opened; 0 when the song is another player's.
DhPlayer* dh_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
