// SPDX-License-Identifier: MIT
// Kris Hatlelid: the memory the driver works in, the known driver versions, the song and instrument records and the
// loader of the executables that hold them.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/players/krishatlelid.h>

// The address of the first loaded hunk. Every hunk is above 64K, as under UADE: the driver tests the upper word of a
// sample address for zero.
#define KH_LOAD_BASE 0x10000u

#define KH_MAX_HUNKS 3

// Memory: the hunks of the song, loaded and relocated. Addresses are the ones the driver sees; reads outside the hunks
// give 0 and writes are dropped.

uint8_t kh_u8(const KhMemory* memory, uint32_t address);
uint16_t kh_u16(const KhMemory* memory, uint32_t address);
int16_t kh_s16(const KhMemory* memory, uint32_t address);
uint32_t kh_u32(const KhMemory* memory, uint32_t address);
void kh_put_u16(KhMemory* memory, uint32_t address, uint16_t value);
void kh_put_u32(KhMemory* memory, uint32_t address, uint32_t value);

// What AUDxLC takes for `address`: null when it is outside the hunks.
const int8_t* kh_chip_pointer(const KhMemory* memory, uint32_t address);

// Records of the song

#define KH_INSTRUMENT_TABLE 0x10u // the instrument records are listed from here in the song
#define KH_ARPEGGIO_STEPS 4

// An instrument record. The first three fields are written by the song start from the IFF data it points to; the
// vibrato counter and the arpeggio position are the driver's state.
typedef struct KhInstrument {
    uint32_t vhdr;                        // 0x00: address of the VHDR chunk's data
    uint32_t body;                        // 0x04: address of the BODY chunk's data
    uint16_t octave_span;                 // 0x08: semitones per octave step
    uint32_t form;                        // 0x0A: address of the FORM
    uint16_t vibrato_max;                 // 0x12: the vibrato turns around above this distance from the note's period
    uint16_t vibrato_min;                 // 0x14: and below this one (a negative distance as a 16 bit value)
    int16_t vibrato_step;                 // 0x16: period change per step
    uint16_t vibrato_rate;                // 0x18: ticks per step
    uint16_t vibrato_delay;               // 0x1A: ticks before the first step
    uint16_t vibrato_counter;             // 0x1C
    uint16_t vibrato_on;                  // 0x1E
    uint16_t arpeggio_on;                 // 0x22
    uint16_t arpeggio[KH_ARPEGGIO_STEPS]; // 0x24
    uint16_t arpeggio_pos;                // 0x2C
    int16_t transpose;                    // 0x2E
} KhInstrument;

KhInstrument kh_instrument_read(const KhMemory* memory, uint32_t address);
void kh_instrument_write_vibrato_counter(KhMemory* memory, uint32_t address, uint16_t value);
void kh_instrument_write_arpeggio_pos(KhMemory* memory, uint32_t address, uint16_t value);
void kh_instrument_write_iff(KhMemory* memory, uint32_t address, uint32_t vhdr, uint32_t body, uint16_t octave_span);

// The VHDR chunk of an 8SVX.
typedef struct KhVhdr {
    uint32_t one_shot; // samples of the part that plays once, at the highest octave
    uint32_t repeat;   // samples of the part that repeats
    uint32_t cycle;    // samples per cycle at the highest octave
    uint8_t octaves;   // octaves in the BODY
} KhVhdr;

KhVhdr kh_vhdr_read(const KhMemory* memory, uint32_t address);

// A track list entry: the stream of one pattern and the transposition of the notes in it.
typedef struct KhListEntry {
    uint32_t stream;
    int16_t transpose;
} KhListEntry;

#define KH_LIST_ENTRY_BYTES 6u

KhListEntry kh_list_entry_read(const KhMemory* memory, uint32_t list, uint32_t offset);

// The song's four track lists, from the song address.
uint32_t kh_song_list(const KhMemory* memory, uint32_t song, uint32_t voice);

// The instrument record at `index` of the song's table; 0 where the table ends.
uint32_t kh_song_instrument(const KhMemory* memory, uint32_t song, uint32_t index);

// The song file

// Offsets of the driver's tables in its data hunk
#define KH_PERIOD_TABLE 0x134u   // 4 byte period constants, 60 entries below this one
#define KH_DURATION_TABLE 0x254u // pairs of bytes: the length of a note and the length of its gate

// The driver's data hunk as it was loaded; bytes past it read 0
uint8_t kh_driver_u8(const KhModule* module, uint32_t offset);
uint32_t kh_driver_u32(const KhModule* module, uint32_t offset);

// Loads and checks the song, and its external driver through `io` (null: only the song). `data` is not kept.
KhStatus kh_module_load(KhModule* module, const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void kh_module_free(KhModule* module);

// The address of a subsong's song record.
uint32_t kh_module_song(const KhModule* module, const KhMemory* memory, uint32_t subsong);

// The words the search for the IFF chunks of all instruments may look at
#define KH_PREPARE_BUDGET_WORDS 0x1000000ull

// The song start's pass over the instrument table from entry `first`: the VHDR and BODY addresses of every instrument
// go into its record. False when an instrument has no VHDR or BODY chunk (the driver would search forever) or no
// octaves, or when the search would take more than `budget` words.
// The first table entry the song start prepares for a driver version (null: the first entry every version prepares).
uint32_t kh_first_prepared_instrument(const KhVariant* variant);

bool kh_prepare_instruments(KhMemory* memory, uint32_t song, uint32_t first, uint64_t* budget);

// Layout of the first hunk of a song file (the eagleplayer's Check2).
typedef enum KhLayout {
    KhLayout_None = 0,
    KhLayout_Embedded, // the driver is in the song
    KhLayout_External, // a stub that returns the song table; the driver is "songplay"
    KhLayout_ExternalShort,
} KhLayout;

KhLayout kh_layout(const uint8_t* data, size_t size);
