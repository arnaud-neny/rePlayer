// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the 8 voice replayer ("MusicMaker-8V"). It mixes its eight voices into four buffers in software
// and plays them on Paula's four channels; the audio interrupt at the end of the buffers drives the song.

#pragma once

#include "mm_core.h"

#define MM8_SILENT_SLACK 2 // words by which a silent block can differ from its nominal length (the carry)

bool mm8_start(Mm8* e, const MmSong* song, const MmInstruments* inst, NuadeVoices* voices);
void mm8_stop(Mm8* e);
MmTickResult mm8_tick(Mm8* e, NuadeVoices* voices);

// The two voices of a channel's pair: A (the odd voice, which the mixer reads first) and B.
typedef enum Mm8PairVoice {
    MM8_VOICE_A = 0,
    MM8_VOICE_B = 1,
} Mm8PairVoice;

// The number of the voice (track) that plays as `voice` of channel `channel`'s pair.
static inline uint32_t mm8_pair_voice_index(uint32_t channel, int voice) {
    return 2 * channel + 1 - (uint32_t)voice;
}

// The length of a silent block (words): a quarter of the tempo.
static inline uint16_t mm8_silent_words(uint16_t tempo) {
    return tempo >> 2;
}

// The mixer: the next block of channel `c` from its voice pair (voices 2c + 1 and 2c), into the channel's buffer; the
// period, volume and length of the block are left in the channel.
void mm8_mix_channel(Mm8* e, uint32_t c);

// Rows of the volume table: row v scales a sample to v / 64.
void mm8_build_tables(int8_t* tables);
