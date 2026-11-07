// SPDX-License-Identifier: MIT
// Paul Summers player (.snk), translated from the UADE eagleplayer "PaulSummers" (Wanted Team). A song is the memory
// image of an Amiga game (Mick Chilton / Paul Summers): its own code is not used. The eagleplayer finds the song table,
// the period table and the subsong count in the image by code patterns and runs its own copy of the game's sound
// engine over them: four voices, each reading a command stream (notes, loops, calls, slides) per tick.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_VOICES 4

// The CIA clock: the timebase of the tick length.
#define PS_TIMEBASE_HZ 709379

#define PS_STACK_WORDS 32u // the 128 bytes a voice has for calls and loops

typedef struct PsPlayer PsPlayer;

typedef struct PsTickResult {
    uint32_t length; // until the next tick, in 1 / PS_TIMEBASE_HZ seconds
    bool looped;     // the eagleplayer calls SongEnd: every voice's data has reached its end command
    bool ended;      // every voice has stopped (or crafted data): later ticks do nothing
} PsTickResult;

typedef struct PsVoiceState {
    int period;     // the period of the last note started, 0 if none
    int instrument; // the index of the instrument range of that note, -1 if none
} PsVoiceState;

typedef struct PsVoice {
    NuadePaulaChannel paula;
    uint32_t stream; // the next byte of the command stream
    uint32_t stack[PS_STACK_WORDS];
    uint32_t stack_used;  // words in use
    uint16_t length;      // ticks left of the note
    uint16_t length_high; // the high byte of the next length, set by a command
    bool stopped;         // the end command was read
    bool stop_done;       // ... and the DMA was switched off
    bool note_active;     // the note is playing: the stream is read when its length has run out
    bool keep_off;        // the voice was switched off by a rest, DMA stays off until the next note
    bool sliding;         // a slide is set
    uint32_t script;      // the stream position the slide reads its next parameters from
    uint16_t slide_delay;
    uint8_t slide_step;
    uint8_t slide_steps;
    uint8_t slide_divisor;
    int32_t note; // the period table index of the playing note
    bool has_ranges;
    uint32_t ranges;
    int period;     // for the display
    int instrument; // for the display
} PsVoice;

typedef struct PsModule {
    uint8_t* mem; // the image with chip memory padding behind it; the init patch is applied
    uint32_t size;
    uint32_t song_table;
    uint32_t period_table; // the table is read downwards from here
    uint32_t subsong_count;
} PsModule;

struct PsPlayer {
    PsModule module;
    uint32_t subsong; // 1-based
    uint8_t tempo;
    uint8_t tempo_counter;
    uint32_t timer;
    bool restart_pending; // the first tick starts the subsong
    bool song_flag[PS_VOICES];
    bool song_end;  // SongEnd was called in this tick
    bool fault;     // the data led out of what the port models
    uint32_t steps; // commands the voice being run has read in this tick
    PsVoice voices[PS_VOICES];
};

// The eagleplayer's Check2, plus what this port needs: the locators find their code in the image, the subsong count is
// in range and no subsong uses a tempo the engine does not survive.
bool ps_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `ps_probe` rejects it.
PsPlayer* ps_create(const uint8_t* data, size_t size);
void ps_destroy(PsPlayer* player);

uint32_t ps_subsong_count(const PsPlayer* player);

// (Re)start a subsong (0-based; the engine numbers them from 1); the player drives `voices` (Paula channel n = voice
// n).
bool ps_start(PsPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
PsTickResult ps_tick(PsPlayer* player, NuadeVoices* voices);

PsVoiceState ps_voice_state(const PsPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PsPlayer* ps_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
