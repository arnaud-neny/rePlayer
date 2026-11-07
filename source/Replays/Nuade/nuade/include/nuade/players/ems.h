// SPDX-License-Identifier: MIT
// EMS player (Editeur Musical Sequentiel 1.24, Thomas "Tom" Pimmel, 1989-91), translated from the UADE eagleplayer
// "EMS" (Wanted Team). A module is a header with a position list and fifteen instruments, 64-row patterns of four
// voices, and the samples after them.

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

#define EMS_VOICES 4
#define EMS_ROWS 64
#define EMS_INSTRUMENTS 15

// The CIA clock: the timebase of EMS_TICK_LENGTH.
#define EMS_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / EMS_TIMEBASE_HZ seconds.
#define EMS_TICK_LENGTH 14187

#define EMS_POSITION_LIST_SIZE 0x80

typedef struct EmsPlayer EmsPlayer;

typedef struct EmsTickResult {
    bool looped; // the position list wrapped (the eagleplayer's SongEnd)
} EmsTickResult;

typedef struct EmsPosition {
    int order; // position in the position list
    int row;   // the row decoded by the last row tick
    int speed; // ticks per row
} EmsPosition;

typedef struct EmsVoiceState {
    int period;     // period of the last note started, 0 if none
    int instrument; // 1-based instrument of the last cell that named one, 0 if none
} EmsVoiceState;

typedef enum EmsEffect {
    EmsEffect_None = 0,
    EmsEffect_VolumeSlide = 7, // volume -= param each tick until it reaches 0
    EmsEffect_Break = 8,       // the next row is row `param` of the next position
    EmsEffect_Speed = 9,       // param 0 does nothing here (the replayer crashes on it)
    EmsEffect_Filter = 10,     // param 0 clears, anything else sets the Amiga's LED filter
    EmsEffect_SlideUp = 11,    // period -= param each tick, down to a period of 0x70
    EmsEffect_SlideDown = 12,  // period += param each tick, up to a period of 0x6B0
    EmsEffect_Vibrato = 13,    // period -/+ param on even/odd ticks
    EmsEffect_SetVolume = 14,
    EmsEffect_Restart = 15, // the whole sample repeats instead of the instrument's loop
} EmsEffect;

// One pattern cell as stored.
typedef struct EmsCell {
    uint16_t period;
    uint8_t instrument; // 1-based, 0 none
    uint8_t effect;     // EmsEffect (1 .. 6 do nothing)
    uint8_t param;
} EmsCell;

typedef struct EmsInstrument {
    const int8_t* sample; // 0 for an instrument without data
    uint16_t length;      // words
    uint16_t loop_start;  // words
    uint16_t loop_length; // words
    uint16_t volume;
} EmsInstrument;

typedef struct EmsModule {
    uint8_t* data; // the file as loaded, in its own allocation with zeros after the end
    uint32_t size;
    int position_count;
    uint8_t positions[EMS_POSITION_LIST_SIZE]; // pattern numbers
    EmsInstrument instruments[EMS_INSTRUMENTS];
} EmsModule;

typedef struct EmsVoice {
    const int8_t* sample; // the sample to start at the next tick's repeat write; 0 when none is pending
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
    uint16_t volume; // as the replayer keeps it; AUDxVOL is its low seven bits
    uint16_t period;
    EmsEffect effect; // the effect of this row's cell that acts on every tick
    uint8_t param;
    int instrument; // display
    NuadePaulaChannel paula;
} EmsVoice;

typedef struct EmsSong {
    uint16_t tick; // ticks since the last row
    uint16_t speed;
    uint16_t row;
    uint16_t break_row; // the row the next position starts at
    uint16_t position;
    uint32_t pattern_offset;
    bool looped;
    int shown_position;
    int shown_row;
    EmsVoice voices[EMS_VOICES];
} EmsSong;

struct EmsPlayer {
    EmsModule module;
    EmsSong song;
};

// The eagleplayer's Check2: the header fields are in range and the first pattern uses only periods of the table. A
// file shorter than the header says is accepted (what is missing reads as zeros), as UADE plays it.
bool ems_probe(const uint8_t* data, size_t size);

// 0 when the module is not accepted by ems_probe or memory runs out. The player keeps its own copy of the data.
EmsPlayer* ems_create(const uint8_t* data, size_t size);
void ems_destroy(EmsPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ems_start(EmsPlayer* player, NuadeVoices* voices);

EmsTickResult ems_tick(EmsPlayer* player, NuadeVoices* voices);

EmsPosition ems_position(const EmsPlayer* player);
int ems_order_count(const EmsPlayer* player);

EmsVoiceState ems_voice_state(const EmsPlayer* player, int voice);

// `order` 0 .. ems_order_count - 1, `voice` 0..3, `row` 0 .. EMS_ROWS - 1.
EmsCell ems_pattern_cell(const EmsPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
EmsPlayer* ems_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
