// SPDX-License-Identifier: MIT
// UFO player: the types shared by the song loader, the sequencer and the voices.
//
// A song is a chunked file (FORM/DDAT/BODY, then CHAN, BPM, DRUM, BLCK, INST, END): 16 logical channels of event
// streams that the sequencer steps, reusable blocks, and instruments that live in the song or in the sample bank. The
// logical channels play on the four Paula voices (channel n on voice n & 3). The eagleplayer's replayer keeps its state
// in one data block; here it is split into the immutable song (UfoSong) and the running state (UfoState).

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/ufo.h>

#define UFO_FIRST_CHUNK 0x14 // after FORM, size, DDAT, BODY, size

// The period table has 84 entries; a note reads the entry after the next one, so the highest note index is 81.
#define UFO_PERIOD_ENTRIES 84
#define UFO_MAX_NOTE_INDEX (UFO_PERIOD_ENTRIES - 3)

// Song

// Chunk walk of the eagleplayer's InitSound. False for a song the replayer would read outside of.
bool ufo_song_parse(UfoSong* song, const uint8_t* data, size_t size);

// A byte of the song file; a read past the end sets *fault and gives 0 (the replayer would read whatever follows).
uint8_t ufo_song_u8(const UfoSong* song, uint32_t offset, bool* fault);

// The variable length delay at *cursor (7 bits per byte, high bit: more follow; 16 bit arithmetic), advancing it and
// adding one to *work per byte.
uint16_t ufo_song_read_delay(const UfoSong* song, uint32_t* cursor, bool* fault, uint64_t* work);

// The eagleplayer's InitPlayer on the sample file: the instrument headers, the sample data and the zeroed first long.
// `bank` is a nuade_paula_padded_copy of the file (the first four bytes are cleared here), `module` one of the song
// file.
bool ufo_bank_load(UfoSong* song, uint8_t* module, uint8_t* bank, uint32_t bank_size);

// The sample file of a song (ExtLoad): a nuade_paula_padded_copy of "<name>.BANK" for "<name>.mus" or else of "SMP.set"
// in the song's directory, its size in *size; null when neither can be read.
uint8_t* ufo_bank_read(const char* path, const NuadeIo* io, uint32_t* size);

// Playable form of a sample record in `buffer` (the song or the bank, a Paula padded copy); false unless it fits.
bool ufo_instrument_make(UfoInstrument* out, const uint8_t* buffer, uint32_t buffer_size, uint32_t sample_offset,
                         uint32_t length, uint32_t loop_offset, uint8_t volume);

// State

void ufo_state_init(UfoState* state, const UfoSong* song);

// One interrupt of the eagleplayer: two sequencer calls, then the volumes.
void ufo_state_tick(const UfoSong* song, UfoState* state);

// Do the two states hold the same sequencer and channel state (everything that decides which song bytes get read)?
bool ufo_state_same_sequence(const UfoState* a, const UfoState* b);

// Sequencer (ufo_sequencer.c) and voices (ufo_voice.c)

void ufo_sequencer_call(const UfoSong* song, UfoState* state);

void ufo_voice_note(const UfoSong* song, UfoState* state, unsigned channel, uint8_t note, uint8_t volume);
void ufo_voice_update_bend(UfoState* state, unsigned channel);
void ufo_voices_update_volumes(UfoState* state);
