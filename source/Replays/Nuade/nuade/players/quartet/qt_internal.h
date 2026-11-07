// SPDX-License-Identifier: MIT
// Quartet: the song file, the sample set, their typed record readers and the replayer's state.
//
// Song file (big-endian): u8 tempo (ticks per row; a divisor of 3000, at most 30), u8 0x50, then rows of 8 bytes,
// four voice words each, from offset 2. The song ends at the first row that has a 0xFFFF word; the file ends with four
// 0xFFFF words (and up to 15 zero words).
// Voice word: byte 0 = octave << 6 | (volume - 1), byte 1 = sample << 4 | note. Byte 0 = 0 or byte 1 = 0xFE starts no
// note. A byte 1 of 0xFE in the next row starts a slide to the next note of this voice that has volume bits.
//
// Sample set (SMP.<name> or SMP.set): 0x162 bytes header ("lock" at offset 4, the loop mask word at 0x150), the
// samples one after the other (8-bit signed; length + 2 words each), 16 length words, 16 (offset, length) word pairs.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_paula_channel.h"
#include <nuade/players/quartet.h>

#define QT_ROW_BYTES 8
#define QT_NOTES 16

typedef struct QtVoiceWord {
    uint8_t octave;
    uint8_t volume_code; // volume - 1; a slide looks for the next word where this is not 0
    uint8_t sample;
    uint8_t note;      // 0..15; 0 and 13..15 read past the period table
    bool plays;        // starts a note
    bool glide_marker; // byte 1 = 0xFE
} QtVoiceWord;

// The readers return false when the record is not inside the file.
bool qt_module_voice_word(const QtModule* module, uint32_t row, uint32_t voice, QtVoiceWord* out);
bool qt_module_row_ends(const QtModule* module, uint32_t row, bool* ends);

// AUDxLC for a sample set offset (bit 0 dropped as the hardware does) when `length_words` (0 = 65536) fit in the set.
const int8_t* qt_samples_lc(const QtSamples* samples, uint32_t offset, uint16_t length_words);

// The sample set's checks and layout (InitPlayer); false when the replayer would not accept the set.
bool qt_samples_parse(QtSamples* samples, const uint8_t* file, uint32_t size);

// The replayer

// Whether the whole song only does what the replayer model covers (a dry run of the rows, no audio). Without a sample
// set only the song's own reads are checked.
bool qt_song_modeled(const QtModule* module, const QtSamples* samples);
