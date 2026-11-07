// SPDX-License-Identifier: MIT
// Silmarils player: types shared by the container, the script interpreter and the music player.
//
// A module is a file with a table of blocks. Sample blocks hold the instruments (a 0x10 byte header, then 8-bit data),
// music blocks hold a command stream. The module also carries a small script (from byte 0x18 of the file) that the
// eagleplayer runs when the song is set up: it names the samples of the 32 instrument slots and picks the music block
// and tempo of each subsong. A module with at most two sample blocks has its samples in a main file, the song's file
// name up to its first '_'. Addresses below are offsets into one memory that holds the song file, then the main file,
// the way the Amiga holds them.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/silmarils.h>

#define SIL_SAMPLE_HEADER 0x10
#define SIL_SCRIPT_START 0x18 // from a module's base: where its script starts

// Memory: a read past the end is 0, a write past the end is dropped. `data` is a nuade_paula_padded_copy, so any AUDxLC
// inside `size` can play any length.

bool sil_memory_init(SilMemory* memory, uint32_t size);
void sil_memory_free(SilMemory* memory);

// True when [address, address + length) lies inside the memory.
bool sil_in_memory(const SilMemory* memory, uint64_t address, uint64_t length);

uint8_t sil_u8(const SilMemory* memory, uint32_t address);
uint16_t sil_u16(const SilMemory* memory, uint32_t address);
uint32_t sil_u32(const SilMemory* memory, uint32_t address);
void sil_put8(SilMemory* memory, uint32_t address, uint8_t value);

// Container

// The 0x10 byte header of a block. A sample block's data follows it; a music block's stream starts at +6.
typedef enum SilBlockType {
    SilBlockType_MusicA = 0,
    SilBlockType_SampleA = 1,
    SilBlockType_SampleB = 2,
    SilBlockType_MusicB = 3,
} SilBlockType;

typedef struct SilBlock {
    SilBlockType type;   // any other value is neither a sample nor music
    uint32_t size;       // of the block, header included
    uint16_t mark;       // 0xFF02 for a music block
    bool packed;         // a sample block that the unpacker has to unpack
    uint32_t loop_start; // a sample's loop; 0 = none
    uint32_t loop_end;   // 0 = the data ends where the block does
} SilBlock;

// False when the header is not inside the memory.
bool sil_block_read(const SilMemory* memory, uint32_t address, SilBlock* block);

bool sil_block_is_sample(const SilBlock* block);
bool sil_block_is_music(const SilBlock* block);

// The eagleplayer's check and the sums of its InitPlayer, for the bytes of one file. `need_music`: the song file must
// have a music block (the main file need not). False when the file is not a container or a block lies outside it.
bool sil_container_parse(const uint8_t* file, uint32_t size, bool need_music, SilContainer* out);

// Address of the module base the file's offsets count from.
uint32_t sil_image_base(const SilImage* image);

// Address of block `index` of the image; false when the index is past the table.
bool sil_image_block(const SilMemory* memory, const SilImage* image, uint32_t index, uint32_t* address);

// Unpacks the packed sample blocks of the image in place (the eagleplayer's depacker); false when a block is not
// inside the image.
bool sil_image_unpack(SilMemory* memory, const SilImage* image);

SilPatch sil_patch_find(const SilMemory* memory, const SilImage* song);

// Script interpreter

void sil_script_init(SilScript* script, SilMemory* memory, const SilImage* song, uint32_t* steps_left);

// The three ways the eagleplayer starts the interpreter. Each returns false when the script fails, leaves the
// supported subset or runs out of its step budget.
// Song script: clears the variables, sets the flag variable to `flag + subsong`, runs the script at the song's byte
// 0x18.
bool sil_script_run_song(SilScript* script, uint16_t flag, uint16_t subsong);
// The main file's script (variables are not cleared).
bool sil_script_run_main(SilScript* script, const SilImage* main);
// Goes on with the script where the last run ended.
bool sil_script_continue(SilScript* script);

// InitPlayer's checks after the first run.
bool sil_script_slots_unset(const SilScript* script);
// Whether the song's script starts with a sample statement for slot 0 (looked for from the start of the file, prefix or
// not).
bool sil_script_starts_with_sample(const SilScript* script);
void sil_script_note_parameters(SilScript* script);

// Music player

typedef enum SilBatch {
    SilBatch_Wait = 0, // a wait byte ended the batch
    SilBatch_End,      // the stream's end command
    SilBatch_Error,    // a byte the port does not model
} SilBatch;

void sil_music_start(SilMusic* music, SilMemory* memory, const SilSlot* slots, uint32_t stream, uint8_t tempo,
                     bool slot_parameters);

// One tick: the wait counter, the commands that fall due, then the DMA starts. False when the music leaves what the
// port models. `looped` is set when the stream ended (and restarted) in the tick.
bool sil_music_tick(SilMusic* music, const NuadeVoices* voices, bool* looped);

// The tick's hand-off of the channels' registers to the voices.
void sil_music_sync(SilMusic* music, NuadeVoices* voices);

// Runs the stream pass after pass without a clock until the state a pass starts with has been seen before: every
// command must be one the port models and every pass must wait. Fails when no state repeats within 8 passes, or when
// `budget` (stream bytes read) is used up.
bool sil_music_validate(SilMusic* music, uint32_t* budget);
