// SPDX-License-Identifier: MIT
// Synth Dream player: types shared by the module readers (sdr_module.c), the tables (sdr_tables.c) and the replayer
// (synthdream.c).
//
// The original addresses everything as base + offset in the 68000's memory: the module, the sample file and its own
// data. The offsets stay uint32_t so that they wrap as the 68000's registers do.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/synthdream.h>

#define SDR_WAVE_WORDS (SDR_WAVE_BYTES / 2)
#define SDR_WAVE_PRESETS 16

// The memory the replayer addresses. UADE loads the module at SDR_MODULE_BASE and the sample file behind it; the rest
// of the chip memory the song can reach is zero, except UADE's own data below the module. A read there is something
// this port does not model: it sets `stray`, and a module whose playback does that is refused.

#define SDR_MODULE_BASE 0x11000u

// Bytes at a module offset (an address less SDR_MODULE_BASE, as the replayer adds the base itself).
uint8_t sdr_u8(SdrMemory* memory, uint32_t offset);
uint16_t sdr_u16(SdrMemory* memory, uint32_t offset);
uint32_t sdr_u32(SdrMemory* memory, uint32_t offset);

// The sample the channel plays from a sample file offset: null when the memory there is not modelled (`stray` set).
// Memory outside the sample file is zero.
const int8_t* sdr_sample_pointer(SdrMemory* memory, uint32_t offset, uint16_t length_words);

// Module records (sdr_module.c)

SdrHeader sdr_read_header(SdrMemory* memory);
int sdr_module_subsong_count(SdrMemory* memory);
// The offset of a voice's track in a subsong.
uint32_t sdr_read_track_offset(SdrMemory* memory, uint32_t subsong, uint32_t voice);

#define SDR_TRACK_ENTRY_BYTES 12

// A track is a list of 12-byte entries, ended by a first byte of 0xFE (play from the first entry again) or 0xFF.
typedef struct SdrTrackEntry {
    uint32_t commands;     // offset of the entry's command stream
    uint8_t transpose;     // added to every note
    uint8_t repeat;        // plays the stream this many more times
    uint8_t substitute_of; // an instrument number that is played as substitute_by
    uint8_t substitute_by;
    uint8_t volume_cut; // subtracted from every note's volume
} SdrTrackEntry;

SdrTrackEntry sdr_read_track_entry(SdrMemory* memory, uint32_t offset);
uint8_t sdr_track_first_byte(SdrMemory* memory, uint32_t offset);

SdrInstrument sdr_read_instrument(SdrMemory* memory, uint32_t table, uint16_t number);

// ExtLoad: the song's samples. The file name is the song's with "sdr." changed to "smp."; "smp.set" in the same
// directory when that fails.
bool sdr_load_samples(SdrMemory* memory, const char* path, const NuadeIo* io);

bool sdr_check_module(SdrMemory* memory);
uint32_t sdr_sample_base(uint32_t module_size);

// The replayer's tables (sdr_tables.c)

typedef enum SdrRomTable {
    SdrRomTable_Period,     // a note's period, 12 per octave
    SdrRomTable_PeriodDown, // ratios (1.0 = 0x2000) that shorten a period: the pitch rises
    SdrRomTable_PeriodUp,   // ratios that lengthen a period: the pitch falls
} SdrRomTable;

// The word at a byte offset from the start of a table. The tables follow each other in the player's data, and a
// lookup that runs past the end of one reads the next; before the first table and after the last it is zero.
uint16_t sdr_rom_word(SdrRomTable table, int32_t offset, bool* stray);
// A byte of waveform preset `index`: presets 0..15 are pulse waves.
int8_t sdr_wave_preset_byte(uint32_t index, uint32_t position, bool* stray);

// The tick of one voice, and the proof (sdr_proof.c) that the whole song only reads memory the port models.

void sdr_voice_tick(SdrPlayer* player, int voice);
bool sdr_prove_module(const SdrPlayer* player);
