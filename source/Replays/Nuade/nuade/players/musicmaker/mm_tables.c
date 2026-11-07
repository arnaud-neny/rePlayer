// SPDX-License-Identifier: MIT
// MusicMaker V8 player: constant tables of the replayers (the same in both).

#include "mm_internal.h"

// Paula period of each note: 64 steps from C-1 (856) with an interval that shrinks from 24 to 7.
const uint16_t mm_period_table[MM_NOTES] = {
    856, 832, 808, 784, 760, 740, 720, 700, 680, 660, 640, 622, 604, 588, 572, 556, 540, 524, 508, 494, 480, 466,
    452, 440, 428, 416, 404, 392, 380, 370, 360, 350, 340, 330, 320, 311, 302, 294, 286, 278, 270, 262, 254, 247,
    240, 233, 226, 220, 214, 208, 202, 196, 190, 185, 180, 175, 170, 160, 151, 143, 135, 127, 120, 113,
};

// The volume of the 16 levels a note's low nibble picks (0 is a rest). The table is followed in memory by the song's
// tempo, which a glide to the level 16 reads.
const uint16_t mm_volume_levels[MM_VOLUME_LEVELS] = { 0, 1, 2, 3, 5, 7, 11, 16, 22, 28, 34, 40, 46, 52, 58, 64 };

// Divisor (x 1/256) of the volume of a note in the "volume follows pitch" mode: higher notes are quieter.
const uint16_t mm_note_volume_divisor[MM_NOTES] = {
    256, 264, 268, 273, 277, 287, 292, 297, 303, 309, 315, 321, 327, 334, 341, 348, 356, 364, 372, 372, 381, 390,
    399, 399, 409, 420, 420, 431, 431, 442, 442, 455, 468, 468, 468, 481, 481, 481, 496, 496, 496, 512, 512, 512,
    512, 512, 512, 512, 512, 512, 512, 512, 511, 511, 511, 511, 511, 511, 511, 511, 511, 511, 511, 511,
};
