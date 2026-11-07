// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player: the parts of the player and what they share.

#pragma once

#include <nuade/players/hippel7v.h>

#include "../hippel/hippel_effects.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdbool.h>
#include <stdint.h>

#define H7V_PAULA_TRACKS 3 // tracks 0..2 play through their own Paula channels
#define H7V_TRACK_ENTRY_SIZE 4
#define H7V_TRACK_ROW_SIZE (H7V_TRACKS * H7V_TRACK_ENTRY_SIZE) // one track position: an entry for each track

// The eagleplayer's mixing rate in kHz (its configuration file EP-Hippel_7V.cfg; the replayer's own default is 16) and
// the Paula period it plays that at
#define H7V_MIX_RATE 24
#define H7V_MIX_PERIOD 149
// Paula volume of the mixer's channel
#define H7V_MIX_VOLUME 64

// The module

// Track entry effects (the high nibble of the fourth byte)
typedef enum H7vTrackEffect {
    H7vTrackEffect_Stop = 0x8,
    H7vTrackEffect_Tempo = 0xD, // the mixer's buffer length, from the whole fourth byte of the next entry
    H7vTrackEffect_Speed = 0xE,
    H7vTrackEffect_Volume = 0xF,
} H7vTrackEffect;

typedef struct H7vTrackEntry {
    uint8_t pattern;
    uint8_t note_transpose;
    uint8_t instrument_transpose;
    uint8_t effect; // H7vTrackEffect (any other value does nothing)
    uint8_t param;  // the low nibble of the fourth byte
    int8_t tempo;   // the fourth byte as a signed number: the tempo offset when the previous entry's effect is Tempo
} H7vTrackEntry;

// The first track position, which no subsong plays, holds the mixer's settings in its first entry.
typedef struct H7vMixerSettings {
    bool hard_clip;      // the sum of the mixed voices is clipped, else divided by 4
    int8_t tempo_offset; // the buffer length is 256 + this in 256ths of the 50 Hz length
} H7vMixerSettings;

typedef struct H7vSubsong {
    uint16_t first; // first and last track position
    uint16_t last;
    uint16_t speed;
} H7vSubsong;

typedef struct H7vSampleRecord {
    uint32_t start;       // into the sample data
    uint16_t length;      // words
    uint16_t loop_offset; // bytes
    uint16_t loop_length; // words
} H7vSampleRecord;

// AUDxLC/AUDxLEN for a sample: in the module, or one silent word where the sample starts past the module (the original
// reads whatever memory is there).
typedef struct H7vSampleRange {
    const int8_t* lc;
    uint16_t len_words;
} H7vSampleRange;

bool h7v_module_open(H7vModule* module, const uint8_t* data, size_t size);
void h7v_module_close(H7vModule* module);

// Every read of module data is bounded: past the module's end the bytes are 0.
uint8_t h7v_byte(const H7vModule* module, uint32_t offset);
uint16_t h7v_word(const H7vModule* module, uint32_t offset);
uint32_t h7v_long(const H7vModule* module, uint32_t offset);

H7vTrackEntry h7v_track_entry(const H7vModule* module, uint32_t offset);
H7vMixerSettings h7v_mixer_settings(const H7vModule* module);
H7vSubsong h7v_subsong(const H7vModule* module, uint32_t number);
H7vSampleRecord h7v_sample_record(const H7vModule* module, uint8_t number);

// The module offset of a sample's start / of a byte in the sample data
uint32_t h7v_sample_offset(const H7vModule* module, uint64_t start);

H7vSampleRange h7v_sample_range(const H7vModule* module, uint64_t offset, uint16_t len_words);

// The first byte of a sample pack's sample is copied over the second (the player does it when it starts the sample)
void h7v_patch_second_byte(H7vModule* module, uint32_t offset);

// A track: its Paula channel or mixer voice, its sequencers and the state of the note it plays

// Result of the song commands of a track entry that the mixer carries out
typedef struct H7vSequencerOutput {
    bool tempo_changed;
    int8_t tempo_offset;
} H7vSequencerOutput;

void h7v_voices_init(H7vSong* song, const H7vModule* module, const H7vSubsong* subsong);

// One sequencer step of a voice: the next row of its pattern, and at a pattern end the next track entry.
H7vSequencerOutput h7v_sequencer_step(const H7vModule* module, H7vSong* song, H7vVoice* voice);

// AUDxLC / AUDxLEN := the voice's repeat
void h7v_write_repeat(H7vVoice* voice);

// One tick of a voice: the sequences and effects. Leaves AUDxPER / AUDxVOL (and the sample registers) in voice->paula.
void h7v_voice_tick(H7vModule* module, H7vSong* song, H7vVoice* voice);

// The mixer

void h7v_mixer_init(H7vMixer* mixer, bool hard_clip, int8_t tempo_offset);
void h7v_mixer_set_tempo(H7vMixer* mixer, int8_t tempo_offset);

// The part of the replayer's tick that follows the voices' work: switch the voices on or off, and give them the
// sample registers.
void h7v_mixer_voices_update(H7vMixer* mixer, H7vSong* song);

// Steps and volumes from the voices' periods and volumes, then the next buffer
void h7v_mixer_mix(H7vMixer* mixer, const H7vSong* song);
