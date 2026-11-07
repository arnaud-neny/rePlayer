// SPDX-License-Identifier: MIT
// Pierre Adane Packer: the song file, its typed record readers and the replayer's state.
//
// Song file (big-endian, offsets from the start of the file):
//   u16 list[4]                    start of each voice's list of track offsets
//   instrument records at 8 * n    n = 1..15, 8 bytes each, ended by a record whose first byte is 0xFF
//   tracks                         note entries
//   the four lists                 u16 track offsets, one per 64 rows; list 0 has one more entry, 0, that ends the song
// Instrument record: u16 length (words), u16 volume, u16 loop offset (bytes into the sample), u16 loop length (words;
// 1 = no loop).
// Track entry: u16 period, u8 instrument << 4 | effect, u8 param. An entry that starts with 0xFF is a marker
// 0xFF, u8 n that sets the length of this and the following entries to n rows, and the note entry follows it.
// Samples: the sample set holds the samples of the instrument records one after the other.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_paula_channel.h"
#include <nuade/players/pierreadane.h>

#define PA_ROWS_PER_TRACK 64
#define PA_NO_LOOP_WORDS 1 // the loop length of an instrument that plays once

typedef struct PaInstrument {
    uint16_t length_words;
    uint16_t volume;
    uint16_t loop_offset;
    uint16_t loop_length_words;
} PaInstrument;

typedef struct PaTrackEntry {
    bool sets_length; // the entry starts with the 0xFF marker
    uint8_t length;   // rows per entry from here on (the marker's argument)
    PaNote note;
    uint8_t bytes; // size of the entry in the track
} PaTrackEntry;

// The record readers return false when the record is not inside the file.
uint16_t pa_module_list_start(const PaModule* module, uint32_t voice);
bool pa_module_list_entry(const PaModule* module, uint32_t voice, int16_t position, uint16_t* track);
bool pa_module_instrument(const PaModule* module, uint32_t number, PaInstrument* out);
bool pa_module_track_entry(const PaModule* module, int16_t position, PaTrackEntry* out);

// AUDxLC for a sample set offset (bit 0 dropped as the hardware does); null when the offset is outside the set.
const int8_t* pa_samples_lc(const PaSamples* samples, uint32_t offset);

// InitPlayer's sample layout: the offset of every instrument's sample in the set and the end of the last one, as the
// replayer computes them (the upper word of its length register carries over between records). Offsets that no set
// can hold are UINT32_MAX.
void pa_module_sample_layout(const PaModule* module, uint32_t start[PA_MAX_INSTRUMENTS], uint32_t* end);

// Whether the whole song only does what the replayer model covers (a dry run of the tick logic, no audio).
bool pa_song_modeled(const PaModule* module);
