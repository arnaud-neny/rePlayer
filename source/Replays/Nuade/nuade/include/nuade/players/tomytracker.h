// SPDX-License-Identifier: MIT
// Tomy Tracker player (1992), translated from the UADE eagleplayer "Tomy Tracker" (Wanted
// Team). Four voices, 31 instruments of one sample with a loop, 64 row patterns of 16 bytes per row.

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

#define TMY_VOICES 4
#define TMY_ROWS 64

// The CIA clock: the timebase of TMY_TICK_LENGTH.
#define TMY_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / TMY_TIMEBASE_HZ seconds.
#define TMY_TICK_LENGTH 14187

typedef struct TmyPlayer TmyPlayer;

typedef struct TmyTickResult {
    bool looped; // the last order entry finished during this tick (the eagleplayer's SongEnd)
} TmyTickResult;

typedef struct TmyPosition {
    int order; // order list entry of the row shown
    int row;
    int speed; // ticks per row
} TmyPosition;

typedef struct TmyVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // 0-based instrument of the voice, -1 if none
} TmyVoiceState;

// One pattern cell as stored.
typedef struct TmyCell {
    int note;           // semitones from C-0 (856 = C-1 = 12), -1 = no note
    uint8_t instrument; // 1-based, 0 = keep the voice's
    uint8_t effect;     // effect number 0..13 (0 = none)
    uint8_t param;
} TmyCell;

typedef enum TmyEffect {
    TmyEffect_None = 0,
    TmyEffect_TonePortamento = 1, // the original has two numbers for it (4 and 8)
    TmyEffect_TonePortamento2 = 2,
    TmyEffect_SlideUp = 3,
    TmyEffect_SlideDown = 4,
    TmyEffect_Vibrato = 5,
    TmyEffect_VibratoVolumeSlide = 6,
    TmyEffect_VolumeSlide = 7,
    TmyEffect_Arpeggio = 8,
    TmyEffect_Nop = 9, // acts on the row tick only, and does nothing
    TmyEffect_SetVolume = 10,
    TmyEffect_PatternBreak = 11,
    TmyEffect_Filter = 12, // sets the LED filter bit: no audio effect in the A/B setup
    TmyEffect_Speed = 13,
    TmyEffect_Count = 14,
} TmyEffect;

typedef struct TmyModule {
    const uint8_t* data;
    uint32_t size;
} TmyModule;

typedef struct TmyVoice {
    NuadePaulaChannel paula;
    uint8_t instrument;       // 1-based, 0 = none yet
    uint8_t note;             // note byte of the last note started
    TmyEffect effect;         // of the row
    uint8_t param;            // the row's parameter
    uint16_t volume;          // the replayer's volume word (the register takes the low 7 bits)
    int16_t period;           // the base period
    int16_t porta_target;     // 0: no portamento running
    uint8_t porta_speed;      // set by the portamento parameter
    bool porta_down;          // the target is below the period
    uint8_t vibrato_param;    // speed (high nibble) and depth (low nibble)
    uint8_t vibrato_pos;      // 8 bit phase
    const int8_t* loop_start; // written to AUDxLC/AUDxLEN after the DMA start
    uint16_t loop_length_words;
    bool start_dma; // a note started this row
    bool started;   // display only
} TmyVoice;

typedef struct TmySong {
    uint16_t counter; // ticks to the next row
    uint16_t speed;   // ticks per row
    uint32_t order;   // order list entry playing
    uint16_t row_offset;
    bool looped;
    int shown_order;
    int shown_row;
    TmyVoice voices[TMY_VOICES];
} TmySong;

struct TmyPlayer {
    TmyModule module;
    uint8_t* owned; // the module with the Paula tail behind it
    TmySong song;
};

// The eagleplayer's check (sizes of the header, the pattern block and the order list) plus a walk of the song that
// proves every cell the replayer can read is one it handles (a module that fails here is left to UADE).
bool tmy_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer); null if it is not a valid module.
TmyPlayer* tmy_create(const uint8_t* data, size_t size);
void tmy_destroy(TmyPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool tmy_start(TmyPlayer* player, NuadeVoices* voices);

TmyTickResult tmy_tick(TmyPlayer* player, NuadeVoices* voices);

// The row decoded by the last row tick (order 0, row 0 before the first one).
TmyPosition tmy_position(const TmyPlayer* player);
int tmy_order_count(const TmyPlayer* player);

TmyVoiceState tmy_voice_state(const TmyPlayer* player, int voice);

// `order` 0 .. tmy_order_count - 1, `voice` 0..3, `row` 0 .. TMY_ROWS - 1.
TmyCell tmy_pattern_cell(const TmyPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
TmyPlayer* tmy_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
