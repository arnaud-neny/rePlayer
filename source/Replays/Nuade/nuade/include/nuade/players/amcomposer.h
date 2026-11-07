// SPDX-License-Identifier: MIT
// AM-Composer player (Thomas Porschen), translated from the UADE eagleplayer "AM-Composer" (Wanted Team).

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

#define AMC_VOICES 4

// The CIA clock: the timebase of AmcTickResult.length.
#define AMC_TIMEBASE_HZ 709379

// Ticks per sequencer step: the player advances its step counter on every fourth tick.
#define AMC_TICKS_PER_STEP 4

#define AMC_STREAMS_PER_KIND 4
#define AMC_ARPEGGIO_NOTES 8

typedef struct AmcPlayer AmcPlayer;

typedef struct AmcTickResult {
    uint32_t length; // until the next tick, in AMC_TIMEBASE_HZ units (the module's own CIA timer)
    bool looped;     // the song wrapped to its first step during this tick
} AmcTickResult;

typedef struct AmcVoiceState {
    uint16_t period;    // of the last note started on the voice, 0 if none
    uint8_t instrument; // its instrument number (1-based, as the stream has it), 0 if none
} AmcVoiceState;

typedef struct AmcModule {
    const uint8_t* data; // the file; in a player, its padded copy (AmcPlayer.chip)
    size_t size;         // the file's size
    uint16_t steps;
    uint8_t timer;
    uint32_t note_streams[AMC_STREAMS_PER_KIND];   // file offsets of the first entry of each voice's note stream
    uint32_t effect_streams[AMC_STREAMS_PER_KIND]; // the same for the effect streams
    int instrument_count;
} AmcModule;

// Effect 2: the period steps down by `delta` every `rate` ticks, for `remaining` ticks.
typedef struct AmcSlide {
    uint16_t delta;
    uint16_t remaining;
    uint8_t rate;
    uint8_t countdown;
    uint16_t period;
} AmcSlide;

// Effect 4: the period register cycles through eight periods, one per tick, for `ticks` ticks.
typedef struct AmcArpeggio {
    uint8_t ticks;
    uint8_t index;
    uint16_t periods[AMC_ARPEGGIO_NOTES];
} AmcArpeggio;

typedef struct AmcVoice {
    NuadePaulaChannel paula;
    uint32_t note_pos;   // offset of the next entry of the note stream
    uint32_t effect_pos; // offset of the next entry of the effect stream
    bool started;        // a note began on the last step: its DMA starts on the next tick
    const int8_t* loop;  // what the started note repeats; written to the channel after the DMA start
    uint16_t loop_words;
    AmcSlide slide;
    AmcArpeggio arpeggio;
    uint16_t last_period; // display only
    uint8_t last_instrument;
} AmcVoice;

struct AmcPlayer {
    uint8_t*
        chip; // the file and NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes (nuade_paula_padded_copy); module.data points here
    AmcModule module;
    AmcVoice voice[AMC_VOICES];
    uint16_t step;      // the step being played, 1.. steps - 1; 0 right after the song wrapped
    uint8_t step_ticks; // ticks since the last step
};

// The eagleplayer's check: the "AMC V1.2 REPLAY!" mark and a header pointer that fits 16 bits.
bool amc_probe(const uint8_t* data, size_t size);

// Loads the module (InitPlayer's checks: the instrument table fits the file and no sample ends past it).
// The player reads the module in place; `data` must stay valid until amc_destroy.
AmcPlayer* amc_create(const uint8_t* data, size_t size);
void amc_destroy(AmcPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool amc_start(AmcPlayer* player, NuadeVoices* voices);

AmcTickResult amc_tick(AmcPlayer* player, NuadeVoices* voices);

// Instruments in the module's table (the notes number them from 1).
int amc_instrument_count(const AmcPlayer* player);

AmcVoiceState amc_voice_state(const AmcPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
AmcPlayer* amc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
