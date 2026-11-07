// SPDX-License-Identifier: MIT
// Major Tom: the module layout and the memory the replayer reads, shared by the module reader and the replayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/players/majortom.h>

#define MT_NOISE_BYTES 3000
#define MT_PERIOD_WORDS 89

// Where UADE keeps the song in Amiga memory. The replayer adds table values that are not bounded by the module to
// pointers, so the address decides what such a read finds.
#define MT_SONG_BASE 0x11000u

// The offsets of the module's tables, as the eagleplayer's InitSound turns the file's offset table into pointers.
typedef enum MtTableOffset {
    MtTable_SongLength = 0x13a, // byte: positions in the song
    MtTable_Restart = 0x13c,    // byte: the position the song continues at after its last
    MtTable_Speed = 0x13e,      // word: ticks per row, set by the speed effect
    MtTable_Order = 0x140,      // 256 bytes: the pattern of each position
    MtTable_Transpose = 0x240,  // 256 bytes: added to the notes of each position
    MtTable_Instruments = 0x340,
    MtTable_Volume = 0x440, // the four sequence block tables, 64 blocks of 64 bytes each
    MtTable_Wave = 0x1440,
    MtTable_Note = 0x2440,
    MtTable_Period = 0x3440,
    MtTable_Waveforms = 0x4440, // 32-byte waveforms
    MtTable_Patterns = 0x6240,  // 1024 bytes per pattern: 64 rows of four 4-byte cells
    MtTable_End = 0x16240,      // the last offset of the file's table; nothing reads it
} MtTableOffset;

typedef enum MtSequence {
    MtSequence_Volume = 0,
    MtSequence_Wave,
    MtSequence_Note,
    MtSequence_Period,
} MtSequence;

#define MT_WAVE_BYTES 32
#define MT_ROW_BYTES 16
#define MT_PATTERN_BYTES (MT_ROWS * MT_ROW_BYTES)
#define MT_BLOCK_BYTES 64

// An instrument is four sequence block numbers.
typedef struct MtInstrument {
    uint8_t block[MT_SEQUENCES];
} MtInstrument;

// The step value that continues the block at the step given by the next byte.
#define MT_STEP_JUMP 0xf0

// The memory read of the replayer: the byte at `offset` from the start of the song (negative offsets are before it).
// `*unmapped` is set when the address holds something the port does not model (it may be null).
uint8_t mt_read_byte(const MtModule* module, int64_t offset, bool* unmapped);
uint32_t mt_read_long(const MtModule* module, int64_t offset, bool* unmapped);

// Header fields.
bool mt_module_check(const uint8_t* data, size_t size);
uint8_t mt_song_length(const MtModule* module);
uint8_t mt_restart(const MtModule* module);
uint16_t mt_speed(const MtModule* module);
void mt_set_speed(MtModule* module, uint16_t speed);
uint8_t mt_order_pattern(const MtModule* module, uint8_t position);
uint8_t mt_transpose(const MtModule* module, uint8_t position);

// Records.
MtInstrument mt_instrument(const MtModule* module, uint8_t number);
MtBlock mt_block(MtSequence sequence, uint8_t number);
uint8_t mt_block_step(const MtModule* module, MtBlock block, int64_t index, bool* unmapped);
// The offset of a waveform in the song.
uint32_t mt_waveform_offset(uint8_t number);
// The cell of `voice` in `row` of the pattern the replayer reaches for pattern number `pattern`: the number is
// shifted into a 16-bit offset that wraps and is sign-extended, so a number from 32 up points before the patterns.
MtCell mt_row(const MtModule* module, uint8_t pattern, uint32_t row, int voice, bool* unmapped);

extern const uint8_t mt_noise_table[MT_NOISE_BYTES];
extern const uint16_t mt_period_words[MT_PERIOD_WORDS];
