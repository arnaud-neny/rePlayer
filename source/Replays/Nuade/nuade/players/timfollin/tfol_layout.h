// SPDX-License-Identifier: MIT
// Tim Follin player: the layout of the song table, shared by the module readers and the open-time proof.
//
// A module is an Atari ST or Amiga program image whose data is a table of 68k addresses of the original memory layout.
// The eagleplayer adds one offset to every address it reads; here that offset turns an address into a file offset. The
// table (the "song table") holds, from its start:
//     +0x000  one long, unused
//     +0x004  16 songs of 4 voice stream addresses
//     +0x104  16 addresses of an instrument table (one per song; a table holds an address per instrument)
//     +0x144  16 bytes: the voice count of each song
//     +0x194  the relocation list of InitPlayer (longs until one with a zero high word)
// An instrument is a word (the sample length in words), a parameter block and the sample at +0x32.

#pragma once

#define TFOL_SONGS 16
#define TFOL_SILENT_SONG 14
#define TFOL_SONG_VOICES_OFFSET 4
#define TFOL_SONG_RECORD_SIZE 16
#define TFOL_INSTRUMENT_TABLES_OFFSET 0x104
#define TFOL_VOICE_COUNTS_OFFSET 0x144
#define TFOL_TABLE_SIZE 0x154
#define TFOL_RELOCATION_LIST_OFFSET 0x194
#define TFOL_INSTRUMENT_SAMPLE_OFFSET 0x32
// The longs the relocation loop adds the offset to: the first long, 16 songs of 4 streams and 16 instrument tables.
#define TFOL_RELOCATED_LONGS 81
// Files larger than this are refused (offsets are 32 bit).
#define TFOL_MAX_SIZE 0x4000000u
