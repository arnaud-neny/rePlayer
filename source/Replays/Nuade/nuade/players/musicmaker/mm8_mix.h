// SPDX-License-Identifier: MIT
// MusicMaker V8 player: what the mixer's driver (mm8_mixer.c) and the two sets of mixing routines (68020 and 68000)
// share.

#pragma once

#include "mm8.h"

#define MM8_NO_PERIOD 0xFFFF // the period of a voice that has no data
#define MM8_UNIT_RATIO 0x100 // the consumption of a voice that is not stepped (8.8)

// The 68000 routines' stepping of the voices (see mm8_mix68k.c).
typedef struct Mm8Step68k {
    bool stepped;        // the voice advances by `advance` instead of one byte per sample
    bool carry_advance;  // and by one more byte when the fraction of the chunk carries
    uint8_t advance[64]; // bytes the voice moves on after each sample of a chunk of 64
    uint16_t ratio;      // bytes per chunk * 4, as the block accounting uses it (0x100: one byte per sample)
} Mm8Step68k;

typedef struct Mm8Plan68k {
    uint16_t mapped[2]; // the period each voice asks the channel for (its own, or the song's table entry)
    int scaled;         // the voice that goes through the volume table, -1 for none
    uint16_t fraction;  // added to the chunk accumulator at the end of every chunk
    Mm8Step68k step[2];
} Mm8Plan68k;

// The 68020 routines' choice (see mm8_mix020.c): the voice of slot x is copied or scaled, slot y is added; a stepped
// voice is held for several samples.
typedef struct Mm8Plan020 {
    bool use_x;
    bool use_y;
    bool x_slow; // the voice is stepped through at the ratio of the periods
    bool y_slow;
    bool x_table; // slot x goes through the volume table
    int x;        // the stream of slot x: 0 = voice A, 1 = voice B
    int y;
    uint16_t step;
} Mm8Plan020;

// A block being mixed: the two voices of a channel and the routines chosen for them.
typedef struct Mm8Block {
    Mm8* e;
    Mm8Channel* ch;
    const Mm8Voice* rec[2];
    int8_t* out;
    uint16_t channel_period;
    uint16_t need[2];  // bytes of each voice the rest of the block takes
    const int8_t* row; // the volume table row for the voice that is scaled
    Mm8Plan020 plan020;
    Mm8Plan68k plan68k;
} Mm8Block;

// The mixing routines of one family. The driver asks them for the channel period, how many bytes the voices need for a
// block, which routine the voices present call for, how fast each voice goes through its data, and mixes with them.
typedef struct Mm8Mixer {
    // The period of the block: that of the faster voice. `raw_period` is the period the voice that gives it has itself.
    uint16_t (*channel_period)(const Mm8Block* b, uint16_t* raw_period);
    // Whether the voice's own period is the channel period (a voice that is stepped has a longer one).
    bool (*defines_period)(const Mm8Block* b, int voice);
    // The bytes of each voice a block of n samples takes, for the voices present.
    void (*needs)(Mm8Block* b, uint16_t n);
    // The routine for the voices that have data now.
    void (*select)(Mm8Block* b);
    // The consumption of a voice in 8.8: its bytes per sample.
    uint16_t (*consumption)(const Mm8Block* b, int voice);
    // Mixes `m` samples; reports the bytes taken from each voice.
    void (*run)(Mm8Block* b, uint32_t m, uint32_t taken[2]);
} Mm8Mixer;

extern const Mm8Mixer mm8_mixer_020;
extern const Mm8Mixer mm8_mixer_68k;

// Row of the volume table that scales the quieter of two volumes to the louder one.
const int8_t* mm8_volume_row(const Mm8* e, uint16_t va, uint16_t vb);

// Whether the voice of the pair has data.
static inline bool mm8_voice_present(const Mm8Block* b, int voice) {
    return b->ch->stream[voice].remaining != 0;
}
