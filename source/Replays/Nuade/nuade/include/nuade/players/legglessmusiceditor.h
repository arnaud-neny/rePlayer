// SPDX-License-Identifier: MIT
// Leggless Music Editor player (Leggless / Silents UK, 1990), translated from the UADE eagleplayer
// "LegglessMusicEditor" (Wanted Team). A song is one file: the "LME" mark, a table of subsong entries (four position
// list offsets each), the instruments (samples or pulse-width synth waves), the position lists, the patterns and the
// sample data. The eagleplayer holds the replayer; every voice runs its own pattern with its own durations.

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

#define LME_VOICES 4

// The CIA clock: the timebase of LME_TICK_LENGTH.
#define LME_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / LME_TIMEBASE_HZ seconds.
#define LME_TICK_LENGTH 14187

// An arpeggio cycles through the note's own period and up to nine notes of its own.
#define LME_ARPEGGIO_SLOTS 10
// A synth wave is generated in a buffer of this size per voice (up to 64 words).
#define LME_WAVE_BYTES 128

typedef struct LmePlayer LmePlayer;

typedef struct LmeTickResult {
    bool looped; // every position list has reached its end since the last report: the eagleplayer calls SongEnd
    bool ended;  // the song cannot be followed any further (crafted data): later ticks do nothing
} LmeTickResult;

typedef struct LmeVoiceState {
    int period;     // the period of the last note started, 0 if none
    int instrument; // the instrument of that note, -1 if none
} LmeVoiceState;

typedef struct LmeSlideState {
    int16_t delta; // period change per tick, 0 = no slide
    uint16_t steps;
    uint16_t target;
    int16_t delay; // ticks before it starts
} LmeSlideState;

// The arpeggio cycles through its slots; the first is always the voice's current period.
typedef struct LmeArpeggioState {
    uint16_t index;  // slot
    uint16_t length; // slots, 0 = no arpeggio
    uint16_t counter;
    uint16_t reload;
    uint16_t slots[LME_ARPEGGIO_SLOTS];
} LmeArpeggioState;

// A record's wave type: a sample, or a synth wave whose pulse width grows to the maximum, then shrinks to the minimum.
typedef enum LmeWaveType {
    LmeWaveType_Sample = 0,
    LmeWaveType_Growing,
    LmeWaveType_Shrinking,
} LmeWaveType;

// The volume envelope's phases; a record value above Finished does nothing.
typedef enum LmeEnvelopePhase {
    LmeEnvelopePhase_None = 0,
    LmeEnvelopePhase_Attack,
    LmeEnvelopePhase_Hold,
    LmeEnvelopePhase_Decay,
    LmeEnvelopePhase_Finished,
} LmeEnvelopePhase;

typedef struct LmeVibrato {
    int16_t offset;     // current period offset
    int16_t step;       // 0 = no vibrato
    uint16_t direction; // non-zero while the offset falls
    int16_t max;        // the offset turns round on reaching this (compared for equality)
    int16_t min;        // and this
    int16_t delay;      // ticks before it starts
} LmeVibrato;

typedef struct LmeEnvelope {
    LmeEnvelopePhase phase;
    int16_t attack_step;
    int16_t attack_max;
    int16_t hold; // ticks
    int16_t decay_step;
    int16_t decay_min;
} LmeEnvelope;

typedef struct LmeWave {
    LmeWaveType type;
    int16_t width; // bytes of value_a at the start of the wave
    int16_t grow_step;
    int16_t max;
    int16_t shrink_step;
    int16_t min;
    int16_t value_a; // the low bytes are the two sample values
    int16_t value_b;
} LmeWave;

typedef struct LmeRetrigger {
    uint16_t counter;
    uint16_t period; // ticks between restarts of the sample, 0 = never
} LmeRetrigger;

// What an instrument record becomes once a voice plays it.
typedef struct LmeSound {
    const int8_t* start;
    uint16_t length_words;
    uint16_t volume;
    const int8_t* loop;
    uint16_t loop_words;
    LmeVibrato vibrato;
    LmeEnvelope envelope;
    LmeWave wave;
    LmeRetrigger retrigger;
} LmeSound;

typedef struct LmeVoice {
    NuadePaulaChannel paula;
    // sequencer
    uint16_t duration_ticks; // ticks until the next event
    bool in_pattern;
    uint32_t pattern_pos;
    uint32_t list_pos;
    uint32_t list_start;
    bool list_pending; // the position list has not reached its end since the last SongEnd
    int16_t transpose;
    // the voice
    uint16_t instrument;
    bool instrument_selected;
    uint16_t period; // the note's period, moved by the slide
    LmeSlideState slide;
    LmeArpeggioState arpeggio;
    LmeSound sound;
    // registers the next tick writes / DMA the end of this tick switches on
    bool loop_pending;
    const int8_t* loop_lc;
    uint16_t loop_len_words;
    bool dma_start;
    int8_t wave[LME_WAVE_BYTES];
    // display
    int shown_period;
    int shown_instrument;
} LmeVoice;

typedef struct LmeModule {
    const uint8_t* bytes; // the whole file
    size_t size;
    uint32_t subsong_count;
    uint32_t instruments; // file offsets
    uint32_t instrument_count;
    uint32_t pattern_table; // one longword per pattern: its offset from the pattern data
    uint32_t pattern_count;
    uint32_t pattern_data;
    uint32_t samples;
    uint32_t grid_ticks; // set by lme_module_validate
} LmeModule;

struct LmePlayer {
    uint8_t* bytes; // the copy of the file the module points into, with the Paula tail (nuade_paula_padded_copy)
    LmeModule module;
    bool ended;
    LmeVoice voices[LME_VOICES];
};

// The eagleplayer's Check2 (the "LME" mark and a zero longword at 0x24) plus what its InitPlayer checks, plus the
// bounds this port needs: every table inside the file and every pattern, note and instrument the position lists can
// reach (the whole song of every subsong is walked once).
bool lme_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `lme_probe` rejects it.
LmePlayer* lme_create(const uint8_t* data, size_t size);
void lme_destroy(LmePlayer* player);

uint32_t lme_subsong_count(const LmePlayer* player);

// (Re)start a subsong; the player drives `voices` (Paula channel n = voice n).
bool lme_start(LmePlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
LmeTickResult lme_tick(LmePlayer* player, NuadeVoices* voices);

// Ticks per step of the voices' common grid: the greatest common divisor of the durations the song uses.
uint32_t lme_grid_ticks(const LmePlayer* player);

LmeVoiceState lme_voice_state(const LmePlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
LmePlayer* lme_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
