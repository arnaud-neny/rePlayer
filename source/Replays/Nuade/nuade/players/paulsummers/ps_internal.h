// SPDX-License-Identifier: MIT
// Paul Summers: the song image, what the eagleplayer's locators find in it and the typed readers of its records.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/paulsummers.h>

// The song table has 20 byte entries, entry n for subsong n (entry 0 is a dummy): four voice offsets, then the tempo
// and the timer of the song. The engine masks the subsong number with 0xF.
#define PS_MAX_SUBSONGS 15u

// The repeat that every note writes after its DMA start: one word of silence.
#define PS_IDLE_WORDS 1u

// Bytes of one instrument range.
#define PS_RANGE_BYTES 14u

// What the eagleplayer's InitSound reads from a song table entry.
typedef struct PsSongEntry {
    uint8_t tempo;      // the tick pattern: 0 = every tick, negative = skips one tick in -tempo + 1
    uint8_t tempo_init; // the counter the song starts with
    uint16_t timer;     // the interrupt period of the song in CIA ticks, 0 = the default
} PsSongEntry;

// One instrument range of the table that a voice's note selects from (14 bytes). The table is a word count followed
// by the ranges.
typedef struct PsRange {
    int32_t sample_offset; // from the range to the sample
    uint16_t length_words;
    uint16_t base;
    uint16_t low;
    uint16_t high;
} PsRange;

// A pair of consecutive entries of the period table.
typedef struct PsPeriodPair {
    uint32_t current; // its high word is the period of the note
    uint32_t next;    // the one of the next semitone
} PsPeriodPair;

// The eagleplayer's Check2 and InitPlayer. Copies the image; false when a locator does not find its code or the song
// table is not what the port models.
bool ps_module_parse(PsModule* module, const uint8_t* bytes, size_t size);
void ps_module_free(PsModule* module);

// Bytes of the image; false past its end. `at` is an offset from the start of the image and wraps like an address.
bool ps_u8(const PsModule* module, uint32_t at, uint8_t* out);
bool ps_u16(const PsModule* module, uint32_t at, uint16_t* out);
bool ps_u32(const PsModule* module, uint32_t at, uint32_t* out);

// The song table entry of `subsong` (1-based).
bool ps_song_entry(const PsModule* module, uint32_t subsong, PsSongEntry* out);

// The start of the command stream of `voice` in `subsong`; false when the voice has none (its offset is 0 or 0x1ef8, or
// it points to the start of the image or below).
bool ps_voice_stream(const PsModule* module, uint32_t subsong, uint32_t voice, uint32_t* out);

bool ps_range(const PsModule* module, uint32_t at, PsRange* out);

// The pair of the period table for `note` (counted from the end of the table, downwards).
bool ps_period_pair(const PsModule* module, int32_t note, PsPeriodPair* out);

// Silence: the eagleplayer's 4 byte BSS hunk, which the repeat of a note points at.
const int8_t* ps_idle_sample(void);
