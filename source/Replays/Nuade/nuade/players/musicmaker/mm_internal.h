// SPDX-License-Identifier: MIT
// MusicMaker V8 player: types shared by the module reader and the two replayers.

#pragma once

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/musicmaker.h>

#define MM_TABLE_SIZES_BYTES 30
#define MM_EVENT_SIZE 3
#define MM_END_OF_LIST 0x03E7
#define MM_NOTES 64
#define MM_VOLUME_LEVELS 16
#define MM_PERIOD_MIN 113
#define MM_PERIOD_MAX 856
#define MM_BLOCK_PER_TEMPO 0x73 // a tick is tempo * 0x73 colour clocks long; the table phase advances by that / period
#define MM_SILENT_PERIOD 0xE6   // the period of the silent buffer the 8 voice channels start on, and of silent blocks
#define MM_UNIT_SCALE 0x100     // pitch scale 1.0
#define MM_PITCH_SCALE_MAX 0x2000
#define MM_VOLUME_ROWS 64      // rows of the 8 voice mixer's volume table
#define MM_VOLUME_ROW_SIZE 256 // a row maps a sample byte to v / 64 of it
// The tempo the replayers' commands clamp to; a song whose own tempo is outside is not accepted. The 8 voice tempo
// has to keep a block (tempo * 0x73 / 113 samples) inside 16 bits.
#define MM_TEMPO_MIN_4V 0x12C
#define MM_TEMPO_MAX_4V 0xAF0
#define MM_TEMPO_MIN_8V 0x258
#define MM_TEMPO_MAX_8V (0xFFFFu * MM_PERIOD_MIN / MM_BLOCK_PER_TEMPO)

extern const uint16_t mm_period_table[MM_NOTES];
extern const uint16_t mm_volume_levels[MM_VOLUME_LEVELS];
extern const uint16_t mm_note_volume_divisor[MM_NOTES];

// Bytes the replayer reads: the song, the instrument block and the chunks of the files. Reads past the end give 0.

uint8_t mm_u8(const MmBlock* b, uint32_t offset);
uint16_t mm_u16(const MmBlock* b, uint32_t offset);
uint32_t mm_u32(const MmBlock* b, uint32_t offset);

// The song ("SE" header, position lists, patterns)

// The variant the song header names, if it is a song of either (the eagleplayers' Check2 on the song).
bool mm_song_identify(const MmBlock* song, MmVariant* variant);
bool mm_song_open(MmSong* song, const MmBlock* data, MmVariant variant);
void mm_song_close(MmSong* song);
uint32_t mm_song_pattern(const MmSong* song, uint16_t number);

// 8V, pair mixed by the 68000 routines: the channel period a voice of `period` is mixed at (its own when above the
// limit).
uint16_t mm_song_slow_period(const MmSong* song, uint16_t period);

// The instruments as the replayer holds them after loading: a table of sample records, the samples, and the tables
// the "set table" command selects from.

// `halve`: the 8 voice replayer halves every sample (arithmetic shift) so that two voices can be added.
bool mm_instruments_open(MmInstruments* inst, const MmBlock* data, bool packed, bool halve);
void mm_instruments_close(MmInstruments* inst);

// The file (a FORM "MMV8", or an old song with a separate instrument file): the eagleplayers' Check2 and the loading of
// the song and the instruments. `external` is false when no instrument file can be read (the probe): an old song is
// then checked without its instruments.

bool mm_file_open(const NuadeSongSource* source, bool external, MmSong* song, MmInstruments* instruments,
                  MmVariant* variant);
