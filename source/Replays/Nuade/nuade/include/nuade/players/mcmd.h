// SPDX-License-Identifier: MIT
// MCMD player, translated from the replayer in UADE's "MCMD player module V1.0 (15 Feb 93)" (Eagleeye of Defect): the
// sequence engine of Jochen Hippel's COSO player in a module with fixed-size sequences and patterns.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/players/hippel.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCMD_VOICES 4

// The CIA clock: the timebase of MCMD_TICK_LENGTH.
#define MCMD_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MCMD_TIMEBASE_HZ seconds.
#define MCMD_TICK_LENGTH 14187

typedef struct McmdPlayer McmdPlayer;

typedef struct McmdTickResult {
    bool looped; // a voice's track wrapped to the loop position during this tick
    bool ended;  // the song stopped itself (track command 8) or the replayer left its code: silence from now on
} McmdTickResult;

typedef struct McmdVoiceState {
    int note;       // period table index of the note last started, -1 if none
    int instrument; // volume sequence (instrument) number, -1 if none
} McmdVoiceState;

// The module's tables, as offsets into the module.
typedef struct McmdModule {
    uint8_t* data; // owned, NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes follow the module (nuade_paula_padded_copy)
    uint32_t size; // the module's size
    uint32_t fseqs;
    uint32_t vseqs;
    uint32_t patterns;
    uint32_t pattern_size; // bytes of one pattern
    uint32_t tracks;
    uint32_t subsongs;
    uint32_t samples;
    uint32_t sample_data;
} McmdModule;

// The first subsong entry
typedef struct McmdSubsong {
    uint16_t first; // first and last track row
    uint16_t last;
    uint16_t speed;
    uint16_t loop; // the row the tracks restart at
} McmdSubsong;

// A sequence: the module or the replayer's start sequence, and where the sequence begins in it. The replayer loads its
// read pointer from a position word sign-extended (adda.w), then steps the full address.
typedef struct McmdSequence {
    const uint8_t* data;
    uint32_t size;
    uint32_t origin;
} McmdSequence;

typedef struct McmdVoice {
    bool dma_restart;   // a frequency sequence stopped the channel in this tick: DMA goes on again at its end
    uint32_t track;     // module offset of this voice's entry in the first track row of the subsong
    uint32_t track_end; // ... in the row after the last one
    uint16_t track_offset;
    uint32_t pattern; // module offset of the current pattern
    uint16_t pattern_pos;

    uint8_t note;
    uint8_t info;
    uint8_t param; // portamento speed or alternative frequency sequence: the info byte of the row before
    uint8_t instrument_transpose;
    uint8_t note_transpose;
    uint8_t channel_volume; // 0..100
    uint32_t porta;         // the portamento accumulator

    McmdSequence fseq;
    uint16_t fseq_pos;
    uint8_t fseq_wait;
    uint8_t fseq_value; // note offset, or an absolute note with bit 7 set

    McmdSequence vseq;
    uint16_t vseq_pos;
    uint8_t vseq_counter;
    uint8_t vseq_speed;
    uint8_t vseq_wait;
    uint8_t volume;
    bool vseq_period; // this tick's base period is note VSEQ_NOTE_PERIOD_INDEX, not the note's

    HipVibrato vibrato;

    uint8_t sample_number; // the sample the frequency sequence started; 0xFF after E2
    const int8_t* loop;    // AUDxLC for the repeat
    uint16_t loop_length;  // words

    int last_instrument;

    NuadePaulaChannel paula;
} McmdVoice;

typedef struct McmdSong {
    McmdSubsong subsong;
    uint16_t speed_counter;
    bool stopped; // track command 8
    bool crashed; // the replayer left its code: UADE's emulation stops with the tick it happened in
    bool looped;
    McmdVoice voices[MCMD_VOICES];
} McmdSong;

struct McmdPlayer {
    McmdModule module;
    McmdSong song;
};

// "MCMD" at 0 and a layout the replayer can walk: the tables follow each other inside the module and the first subsong
// is a range of track rows. Exactly what mcmd_create accepts.
bool mcmd_probe(const uint8_t* data, size_t size);

// 0 when the data is not a module the port takes (or out of memory).
McmdPlayer* mcmd_create(const uint8_t* data, size_t size);
void mcmd_destroy(McmdPlayer* player);

// The player has one song: its eagleplayer has no subsong range and always plays the first subsong entry.
bool mcmd_start(McmdPlayer* player, NuadeVoices* voices);

McmdTickResult mcmd_tick(McmdPlayer* player, NuadeVoices* voices);

// Ticks per sequencer step.
int mcmd_speed(const McmdPlayer* player);

McmdVoiceState mcmd_voice_state(const McmdPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
McmdPlayer* mcmd_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
