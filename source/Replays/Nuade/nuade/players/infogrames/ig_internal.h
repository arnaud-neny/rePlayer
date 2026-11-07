// SPDX-License-Identifier: MIT
// Infogrames player: types shared by the module loader (ig_module.c) and the replayer (infogrames.c).
//
// The song file holds one header per song, and behind it the tables of the song: amplitude and pitch envelopes (13 byte
// records), a track list per voice (pattern numbers, 0xFF ends it), the pattern offsets and the patterns (notes and
// commands, 0xFF ends one). Offsets in the file are signed 16-bit words, relative to the song header. The samples are
// in the ".ins" file, behind a 4 byte header.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/infogrames.h>

#define IG_ENVELOPE_SEGMENTS 4
#define IG_ENVELOPE_RECORD_SIZE 13
#define IG_SAMPLE_ENTRY_SIZE 16
#define IG_SAMPLE_HEADER_SIZE 4
#define IG_NO_OFFSET (-0x40000000) // an offset far outside the file: reads zeros

// A file of the song as the player keeps it: a read outside returns 0 (the original would read whatever is in memory
// there). The allocation comes from nuade_paula_padded_copy, so a sample that starts inside the file can run past its
// end.

uint8_t ig_u8(const IgFile* f, int32_t offset);
uint16_t ig_u16(const IgFile* f, int32_t offset);
int16_t ig_s16(const IgFile* f, int32_t offset);
uint32_t ig_u32(const IgFile* f, int32_t offset);

// The header of the song that is played: the words that follow the song's offset in the table at the file start.

// The song the eagleplayer plays: the last one of the file's table.
IgSongHeader ig_read_song_header(const IgFile* song);

// One envelope record: bit 7 of the first byte loops the envelope, bits 0-6 are the offset that a loop adds; the
// segments follow.
typedef struct IgEnvelopeRecord {
    bool loops;
    int8_t loop_step;
    int32_t segments; // IG_ENVELOPE_SEGMENTS of: repeats, delta, ticks
} IgEnvelopeRecord;

IgEnvelopeRecord ig_read_envelope_record(const IgFile* song, int32_t table, unsigned index);

// One segment of an envelope: it adds `delta` to the level once per repeat and lasts `ticks` ticks.
typedef struct IgEnvelopeSegment {
    uint8_t repeats;
    int8_t delta;
    uint8_t ticks;
} IgEnvelopeSegment;

IgEnvelopeSegment ig_read_envelope_segment(const IgFile* song, int32_t segments, unsigned index);

// A sample of the ".ins" file: the first run, then the run that repeats.
typedef struct IgSample {
    uint32_t start; // offsets behind the file header
    uint32_t repeat_start;
    uint16_t length_words;
    uint16_t repeat_length_words;
} IgSample;

// False when the entry does not lie inside the file.
bool ig_read_sample(const IgFile* samples, unsigned index, IgSample* out);

// The replayer's state

#define IG_NO_SAMPLE (-1)

// The ".ins" file name candidates for a song file name: the name without ".dum" plus ".ins", then the same with the
// last character of the name dropped. False when the name has no first candidate.
#define IG_CANDIDATES 2
#define IG_NAME_MAX 512
bool ig_sample_file_names(const char* song_name, char names[IG_CANDIDATES][IG_NAME_MAX], int* count);

// The timer the eagleplayer sets for a song: the sum of its longs and its size are looked up in a table of known songs.
uint32_t ig_song_tick_length(const uint8_t* data, size_t size);

// The replayer's period table: a note beyond it reads what follows the table in the eagleplayer (a note above this
// count is not used by any known song).
uint16_t ig_period(unsigned index);
