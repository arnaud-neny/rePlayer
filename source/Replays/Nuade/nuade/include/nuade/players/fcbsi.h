// SPDX-License-Identifier: MIT
// Future Composer BSI ("FUCO" modules, Tony Bybell), translated from the UADE eagleplayer "FutureComposer-BSI". It is
// a different replayer from Future Composer 1.0-1.4 (players/futurecomposer).

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

#define FCBSI_VOICES 4

// The CIA clock: the timebase of FCBSI_TICK_LENGTH.
#define FCBSI_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / FCBSI_TIMEBASE_HZ seconds.
#define FCBSI_TICK_LENGTH 14187

#define FC_INSTRUMENT_COUNT 64

typedef struct FcBsiPlayer FcBsiPlayer;

typedef struct FcBsiTickResult {
    bool ended; // this tick silenced every voice: the tick after the one where the eagleplayer's SongEnd was called
} FcBsiTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct FcBsiVoiceState {
    int note;
    int instrument;
} FcBsiVoiceState;

// How the period moves each step (pattern commands 0x3D..0x3F)
typedef enum FcBsiSlide {
    FcBsiSlide_None,
    FcBsiSlide_Up,   // the period grows by the amount: lower pitch
    FcBsiSlide_Down, // the period shrinks by the amount
} FcBsiSlide;

typedef struct FcBsiVoice {
    NuadePaulaChannel paula;
    uint8_t sequence_pos;
    uint8_t pattern;
    uint16_t pattern_pos; // the low byte alone moves on commands, the whole word on notes
    uint16_t repeat_count;
    int16_t duration_counter;
    uint16_t default_duration;
    FcBsiSlide slide;
    uint16_t slide_amount;
    uint16_t period;
    uint8_t instrument;
    uint8_t transpose;
    uint8_t volume;
    int note; // display only
} FcBsiVoice;

struct FcBsiPlayer {
    uint8_t* chip; // the module followed by zeros, the memory the voices play from
    size_t size;
    uint32_t sample_offset[FC_INSTRUMENT_COUNT]; // where each instrument's sample starts in `chip`
    FcBsiVoice voices[FCBSI_VOICES];
    uint16_t speed;
    int16_t speed_counter;
    uint16_t steps; // fetches the voice that runs has made this tick
    bool stopped;   // SongEnd was called: the next tick silences the voices
};

// The eagleplayer's check ("FUCO" at the start) and the size its tables need.
bool fcbsi_probe(const uint8_t* data, size_t size);

// Null when fcbsi_probe fails or on out of memory.
FcBsiPlayer* fcbsi_create(const uint8_t* data, size_t size);
void fcbsi_destroy(FcBsiPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool fcbsi_start(FcBsiPlayer* player, NuadeVoices* voices);

FcBsiTickResult fcbsi_tick(FcBsiPlayer* player, NuadeVoices* voices);

// Ticks one sequencer step spans at the start of the song (a step is speed + 1 ticks).
uint32_t fcbsi_grid_ticks(void);
FcBsiVoiceState fcbsi_voice_state(const FcBsiPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
FcBsiPlayer* fcbsi_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
