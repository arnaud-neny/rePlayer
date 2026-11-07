// SPDX-License-Identifier: MIT
// Ashley Hogg player: types shared by the module loader and the two driver families.
//
// A module is one game's sound driver, as it sits in the game's memory. There are two families:
//  - the old family (CJ, Nobby, Spikey, Stuntman Seymour): four `bra.w` and the tick routine at the start, the song
//    data and the samples in the same file;
//  - the new family (Cosmic Spacehead, Fantastic Dizzy): seven `bra.w` at the start, the samples in a separate "smp."
//    file.
// The eagleplayer locates the tables in the code, patches the driver (the CIA setup, SongEnd, volume scaling) and calls
// into it. The driver keeps its voice records in its own file; here they are structs, and everything else (tables,
// streams, samples) is read from the module bytes at the offsets the loader found in the code.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/ashleyhogg.h>

#define AH_LAST_VOICE (AH_VOICES - 1)
#define AH_MAX_VOLUME 64
#define AH_VOLUME_MASK 0x7F // the eagleplayer's volume patch keeps 7 bits

// Bytes of the module or of the sample file: a read past the end is 0 (the driver would read whatever follows in
// memory). `capacity` can be larger than `size`, so that a sample may start past the end of the file; the allocation
// is a nuade_paula_padded_copy of `capacity` bytes, so any length can play from an AUDxLC up to `capacity`.

// Loads `size` bytes and zero fills up to `capacity` (at least `size`), then the Paula tail.
bool ah_memory_init(AhMemory* m, const uint8_t* data, size_t size, uint32_t capacity);

uint8_t ah_u8(const AhMemory* m, uint32_t offset);
uint16_t ah_u16(const AhMemory* m, uint32_t offset);
int16_t ah_s16(const AhMemory* m, uint32_t offset);
uint32_t ah_u32(const AhMemory* m, uint32_t offset);

// AUDxLC / AUDxLEN of a run of `m`: AUDxLC ignores its lowest bit, `length_words` goes out as the driver has it (0
// plays 65536 words, past the file zeros). An AUDxLC beyond `capacity` points at memory the port does not have: none.
void ah_set_sample(const AhMemory* m, NuadePaulaChannel* paula, uint32_t offset, uint16_t length_words);

// AUDxVOL of a driver volume: the eagleplayer keeps 7 bits (the channel applies Paula's rule to them).
uint8_t ah_paula_volume(unsigned volume);

// Old family

#define OLD_INSTRUMENT_SIZE 0x2C
#define OLD_SONG_ENTRY_SIZE 16
#define OLD_DEFAULT_MASTER 0x3F

// The instrument record at `offset` of the module.

static inline AhOldInstrument ah_old_read_instrument(const AhMemory* m, uint32_t offset) {
    const int32_t sample = (int32_t)ah_u32(m, offset + 0x20);
    return (AhOldInstrument) {
        .attack_rate = ah_u8(m, offset),
        .sustain_level = ah_u8(m, offset + 0x02),
        .release_rate = ah_u8(m, offset + 0x03),
        .level = ah_u8(m, offset + 0x05),
        .arp_table = ah_u16(m, offset + 0x06),
        .arp_pos = ah_u16(m, offset + 0x08),
        .length_ticks = ah_u16(m, offset + 0x0A),
        .wave_pos = ah_u16(m, offset + 0x0C),
        .wave_step = ah_s16(m, offset + 0x0E),
        .wave_limit_lo = ah_u8(m, offset + 0x10),
        .wave_limit_hi = ah_u8(m, offset + 0x12),
        .slide = ah_s16(m, offset + 0x14),
        .vibrato_count = ah_u8(m, offset + 0x16),
        .vibrato_period = ah_u8(m, offset + 0x17),
        .vibrato_step = ah_s16(m, offset + 0x18),
        .vibrato_delay = ah_u16(m, offset + 0x1A),
        .vibrato_growth = ah_s16(m, offset + 0x1C),
        .vibrato_max = ah_s16(m, offset + 0x1E),
        .wave_table = sample < 0,
        .dma_started = sample == 0,
        .sample = (uint32_t)sample,
        .loop = ah_u32(m, offset + 0x24),
        .length_words = ah_u16(m, offset + 0x28),
        .transpose_note = ah_s16(m, offset + 0x2A) < 0,
    };
}

// New family

#define NEW_PERIOD_ROW_SIZE 0x48
#define NEW_RECORD_SIZE 16 // of a sample in the smp file
#define NEW_DEFAULT_PHASE_LIMIT 6
#define NEW_MAX_SUBSONG 0xFD // song numbers from 0xFE on are special modes of the driver, not songs

// Sample number `number` of the smp file.

static inline AhNewSample ah_new_read_sample(const AhMemory* samples, uint8_t number) {
    const uint32_t at = number * NEW_RECORD_SIZE;
    return (AhNewSample) {
        .start = ah_u32(samples, at),
        .loop_start = ah_u32(samples, at + 4),
        .start_length = ah_u16(samples, at + 8),
        .loop_length = ah_u16(samples, at + 10),
        .volume = ah_u8(samples, at + 13),
        .period_row = ah_u16(samples, at + 14),
    };
}

// Loader (ah_module.c)
bool ah_old_locate(AhOld* old, const AhMemory* module);
bool ah_new_locate(AhNew* modern, const AhMemory* module);

// How far the AUDxLC of a sample can reach (and the bytes the driver reads around them): the module is padded to it.
uint64_t ah_old_samples_end(const AhOld* old, const AhMemory* module);
uint64_t ah_new_samples_end(const AhMemory* samples);

// Old driver (ah_old.c)
void ah_old_start(AhOld* old, const AhMemory* module, int subsong);
void ah_old_tick(AhOld* old, const AhMemory* module);

// New driver (ah_new.c)
void ah_new_start(AhNew* modern, const AhMemory* module, int subsong);
void ah_new_tick(AhNew* modern, const AhMemory* module);
