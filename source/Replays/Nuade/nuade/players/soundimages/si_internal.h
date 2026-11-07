// SPDX-License-Identifier: MIT
// Sound Images: types shared by the module check and the driver.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/players/soundimages.h>

// The module is the driver. It keeps its variables in a header at offset 4 (the register a5 of the code) and its
// four voice records behind the sample table; both are initialised in the file.

#define SI_HEADER_OFFSET 4u
#define SI_HEADER_BYTES 0x16u        // the variables the driver reaches at offsets 0 .. $15 of the header
#define SI_RECORD_BYTES_WIDE 0x48u   // the voice record of most versions
#define SI_RECORD_BYTES_NARROW 0x46u // the older games': the fields from $20 on sit two bytes lower
#define SI_SAMPLE_BYTES 12u          // a sample table entry: offset, length, loop offset, loop length
#define SI_SONG_BYTES 10u            // a song table entry: four list offsets and the tempo
#define SI_MAX_SEGMENTS 12u

// The eagleplayer's Check2: the first word is a `bra.w` to the driver's interrupt entry, behind an `rte`.
#define SI_BRA_W 0x6000u

// An image describes one version of the driver's code.

#define SI_TARGET_FREE (-2) // a target that moves with the data of the module and that the port does not use

extern const SiImage si_images[];
extern const size_t si_image_count;

// The module as the port reads it

// The eagleplayer's Check2 and the proof that the code is one of the images. False if either fails.
bool si_module_open(SiModule* module, const uint8_t* data, size_t size);
void si_module_close(SiModule* module);

// Reads of the module as data. False outside the module and inside the variables of the driver, which the port keeps
// in structures and the original in the module: a read there would see the state of the original.
bool si_module_u8(const SiModule* module, int64_t offset, uint8_t* value);
bool si_module_u16(const SiModule* module, int64_t offset, uint16_t* value);

// One song table entry.
typedef struct SiSong {
    uint16_t list[SI_VOICES]; // each voice's list of sequences, an offset from the song table; 0 for none
    uint8_t tempo;            // ticks per step
} SiSong;

bool si_module_song(const SiModule* module, uint32_t index, SiSong* song);

// The proof that every byte of song data the driver can read lies in the module and clear of the unreadable places.
bool si_module_walk(const SiModule* module);

// One sample table entry.
typedef struct SiSample {
    int64_t offset; // from the start of the module
    uint16_t length_words;
    int64_t loop_offset;
    uint16_t loop_length_words;
} SiSample;

SiSample si_module_sample(const SiModule* module, uint32_t index);

// One voice record as the module holds it at the start: the fields the driver reads before it writes them.
typedef struct SiRecord {
    uint16_t length_words;      // $04
    uint16_t period;            // $06
    uint8_t volume_high;        // $08
    uint8_t volume;             // $09
    uint32_t hardware;          // $0a the address of the channel's registers
    uint8_t duration;           // $17
    uint8_t note;               // $18
    uint8_t note_step;          // $19
    uint16_t loop_length_words; // $20
    uint16_t dma;               // $22 the DMACON word that starts the channel
    uint8_t volume_base;        // $24
    uint8_t attack_step;        // $25
    uint8_t attack_count;       // $26 (the count the note start loads into $2c)
    uint8_t decay_step;         // $27
    uint8_t decay_count;        // $28
    uint8_t release_step;       // $29
    uint8_t release_start;      // $2a
    bool release_on;            // $2b
    uint8_t attack_left;        // $2c
    uint8_t decay_left;         // $2d
    uint8_t vibrato_depth;      // $2e
    uint8_t vibrato_rate;       // $2f
    uint8_t vibrato_left;       // $30
    uint8_t slide_left;         // $31
    uint16_t slide_step;        // $32
    uint16_t bend_offset;       // $34
    uint8_t bend_ticks;         // $36
    uint8_t note_length;        // $37
    uint16_t interrupt;         // $38 the INTENA word that enables the channel's interrupt
    uint8_t attack_wait;        // $3a
    uint8_t attack_rate;        // $3b
    uint8_t decay_wait;         // $3c
    uint8_t decay_rate;         // $3d
    uint8_t release_wait;       // $3e
    uint8_t release_rate;       // $3f
    uint8_t vibrato_delay;      // $40
    uint8_t arpeggio_index;     // $41
    uint8_t transpose;          // $46
    uint8_t active;             // $47 (the driver with voice flags)
} SiRecord;

SiRecord si_module_record(const SiModule* module, uint32_t voice);
