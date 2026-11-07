// SPDX-License-Identifier: MIT
// Fashion Tracker player (Fashion, 1988), translated from the UADE eagleplayer "Fashion Tracker player module V1.0"
// (Wanted Team). The module is an Amiga executable of the original player; the eagleplayer holds the replayer and
// takes the song data out of the module's code.

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

#define FT_VOICES 4
#define FT_ROWS 64

// The CIA clock: the timebase of FT_TICK_LENGTH.
#define FT_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / FT_TIMEBASE_HZ seconds.
#define FT_TICK_LENGTH 14187

typedef struct FtPlayer FtPlayer;

typedef struct FtTickResult {
    bool looped; // the last order entry started during this tick
} FtTickResult;

typedef struct FtPosition {
    int order; // order list entry of the row shown
    int row;
    int speed; // ticks per row
} FtPosition;

typedef struct FtVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // instrument slot (0..15) of the last note started, -1 if none
} FtVoiceState;

// One pattern cell as stored.
typedef struct FtCell {
    uint16_t period;    // 0: no note
    uint8_t instrument; // slot 0..15; 0 = the voice's previous one
    uint8_t effect;     // 1 = arpeggio
    uint8_t param;      // arpeggio: semitones, high nibble on ticks 1 and 4, low nibble on ticks 2 and 5
} FtCell;

typedef struct FtModule {
    uint8_t* data;      // the file, zeros up to the last sample start, then the Paula tail (nuade_paula_padded_copy)
    uint32_t size;      // bytes of `data` before the Paula tail
    uint32_t link_base; // the address the module's code was linked at; table addresses are relative to it
    uint32_t sample_lengths; // offsets of the tables: long per sample
    uint32_t sample_starts;
    uint32_t sample_loop_flags; // the low byte of each long is the flag
    uint32_t instruments;       // 4 bytes per slot: sample number, volume
    uint32_t patterns;
    uint32_t order_list;
    uint32_t order_count;
} FtModule;

typedef struct FtVoice {
    NuadePaulaChannel paula;
    uint16_t last_period; // the period of the last note started (arpeggio base)
    uint8_t instrument;   // instrument slot of the last note started
    bool started;         // display only: a note has started
    int note;             // display only
} FtVoice;

typedef struct FtSong {
    uint8_t tick;    // 0 on a row tick
    uint8_t row;     // the row to play next is row + 1; ROWS_DONE before a new pattern
    uint32_t order;  // the order list entry the next pattern comes from
    uint8_t pattern; // pattern playing
    bool looped;
    int shown_order;
    int shown_row;
    FtVoice voices[FT_VOICES];
} FtSong;

struct FtPlayer {
    FtModule module;
    FtSong song;
};

// The eagleplayer's check: the first instructions of the module's own player.
bool ft_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer); null if it is not a valid module.
FtPlayer* ft_create(const uint8_t* data, size_t size);
void ft_destroy(FtPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ft_start(FtPlayer* player, NuadeVoices* voices);

FtTickResult ft_tick(FtPlayer* player, NuadeVoices* voices);

// The row decoded by the last row tick (order 0, row 0 before the first one).
FtPosition ft_position(const FtPlayer* player);
int ft_order_count(const FtPlayer* player);

FtVoiceState ft_voice_state(const FtPlayer* player, int voice);

// `order` 0 .. ft_order_count - 1, `voice` 0..3, `row` 0 .. FT_ROWS - 1.
FtCell ft_pattern_cell(const FtPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
FtPlayer* ft_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
