// SPDX-License-Identifier: MIT
// GlueMon player (Glue Master / Northstar), translated from the UADE eagleplayer "GlueMon". A song is one file: the
// "GLUE" mark, an 8 byte name, the volume decay of the voices, the speed and row count, four 32 byte waveforms, the
// position table and the 4 byte rows of the patterns. The eagleplayer holds the replayer.

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

#define GM_VOICES 4

// The song name in the header: bytes, not a C string.
#define GM_NAME_LENGTH 8

// The CIA clock: the timebase of GM_TICK_LENGTH.
#define GM_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / GM_TIMEBASE_HZ seconds.
#define GM_TICK_LENGTH 14187

#define GM_NOISE_LONGS 8
#define GM_NOISE_BYTES (GM_NOISE_LONGS * 4)

typedef struct GmPlayer GmPlayer;

typedef struct GmTickResult {
    bool looped; // the position table wrapped again: the eagleplayer calls SongEnd
} GmTickResult;

typedef struct GmPosition {
    uint32_t order; // index in the position table
    uint32_t row;
    uint32_t tick;  // ticks into the row
    uint32_t speed; // ticks per row
} GmPosition;

// What `voice` holds in `row` of the pattern at position table entry `order`: a note (index in the period table), a
// note off, a percussion command byte or nothing.
typedef struct GmCell {
    int note; // period table index, -1 if none
    bool note_off;
    int command; // percussion command byte of the fourth voice, -1 if none
} GmCell;

// The volume decay of a voice: each tick a volume above `threshold` drops by `decrement`; a note sets it to `reload`.
typedef struct GmEnvelope {
    uint8_t reload;
    uint8_t decrement;
    uint8_t threshold;
} GmEnvelope;

typedef struct GmVoice {
    NuadePaulaChannel paula;
    GmEnvelope envelope; // the percussion commands change the decrement of the fourth voice
    uint16_t volume;
    int note; // period table index of the last note, -1 if none
} GmVoice;

typedef struct GmModule {
    const uint8_t* bytes; // the whole file
    size_t size;
    char name[GM_NAME_LENGTH];
    GmEnvelope envelope[GM_VOICES];
    bool percussion;      // the fourth voice plays commands; its envelope is then fixed
    uint8_t speed;        // ticks per row
    uint8_t rows;         // rows per pattern
    uint8_t order_bytes;  // the size of the position table with its end mark; the song starts one entry before the end
    uint32_t order_count; // entries before the first end mark of the position table
    size_t waves;         // file offset of the four waveforms
    size_t orders;        // file offset of the position table
    size_t patterns;      // file offset of the first pattern
} GmModule;

struct GmPlayer {
    uint8_t* bytes; // the copy of the file the module points into
    GmModule module;
    uint8_t tick;    // ticks into the row
    uint16_t row;    // the row the last row tick played
    uint16_t order;  // position table entry of the pattern being played
    uint8_t pattern; // its pattern number
    bool wrapped;    // the table wrapped once (at the start): the next wrap is the song end
    uint16_t percussion_period;
    int16_t percussion_step;
    uint32_t noise_seed;
    int8_t noise[GM_NOISE_BYTES]; // the buffer the noise commands play
    GmVoice voices[GM_VOICES];
};

// The eagleplayer's Check2 (the two marker longwords), plus the bounds this port needs: the position table ends with
// 0xFF, every pattern it names lies inside the file and every note stays inside the period table.
bool gm_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `gm_probe` rejects it.
GmPlayer* gm_create(const uint8_t* data, size_t size);
void gm_destroy(GmPlayer* player);

// (Re)start the song from its first position; the player drives `voices` (Paula channel n = voice n).
bool gm_start(GmPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
GmTickResult gm_tick(GmPlayer* player, NuadeVoices* voices);

// The 8 byte song name as a C string: printable characters only, trailing blanks removed.
void gm_title(const GmPlayer* player, char out[GM_NAME_LENGTH + 1]);

GmPosition gm_position(const GmPlayer* player);
uint32_t gm_order_count(const GmPlayer* player);
uint32_t gm_row_count(const GmPlayer* player);

GmCell gm_cell(const GmPlayer* player, uint32_t order, uint32_t voice, uint32_t row);

// The period table index of the last note a voice started, -1 if none.
int gm_voice_note(const GmPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
GmPlayer* gm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
