// SPDX-License-Identifier: MIT
// Speedy System player (Michael Winterberg's music editor), translated from the UADE eagleplayer "SpeedySystem". A
// song is one file ("ss.<name>" or "<name>.ss"): the header "SPEEDY-SYSTEM", a table of pattern pointers, a table of
// sample pointers and lengths, the patterns and the samples. The eagleplayer holds the replayer.

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

#define SS_VOICES 4

// The CIA clock: the timebase of SS_TICK_LENGTH.
#define SS_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SS_TIMEBASE_HZ seconds.
#define SS_TICK_LENGTH 14187

// Sample numbers a note can start: the cell's third byte 0..0x30; 0x31..0xB0 start nothing.
#define SS_SAMPLES 0x31

typedef struct SsPlayer SsPlayer;

typedef struct SsTickResult {
    bool looped; // the replayer reached the end of the song and restarted: the eagleplayer calls SongEnd
} SsTickResult;

typedef struct SsVoiceState {
    int note;       // semitones from C-0 (12 = C-1) of the last note started, -1 if none
    int instrument; // the sample number of that note, -1 if none
} SsVoiceState;

typedef enum SsEffect {
    SsEffect_None,
    SsEffect_PeriodAdd,
    SsEffect_PeriodSub,
} SsEffect;

typedef struct SsVoice {
    bool active;         // the voice plays: its envelope and slide run
    bool attacking;      // the volume rises to 64
    bool releasing;      // after the attack: the volume may fall
    bool repeat_pending; // the sample has a repeat part that the row's end still has to write
    uint16_t attack_delay;
    uint16_t release_delay;
    uint16_t delay_counter;
    uint16_t volume;
    uint16_t period;
    SsEffect effect;
    uint16_t effect_amount;
    unsigned sample;
    int note;
    int instrument;
} SsVoice;

// A sample table entry as the replayer starts it: what it writes to AUDxLC and AUDxLEN before the DMA start, and the
// repeat it writes after (the sample is an IFF 8SVX file with a one shot part and a repeat part). Offsets are in the
// file.
typedef struct SsSample {
    bool valid; // the sample starts (and repeats) in the file
    uint32_t start;
    uint16_t start_len_words;
    bool has_repeat;
    uint32_t repeat;
    uint16_t repeat_len_words;
} SsSample;

typedef struct SsModule {
    const uint8_t* data; // `size` bytes; the player's copy comes from nuade_paula_padded_copy
    size_t size;
    uint32_t base; // the offset of the marker long
    uint8_t speed; // the byte before the marker: ticks of a row - 1
    SsSample samples[SS_SAMPLES];
} SsModule;

struct SsPlayer {
    uint8_t* image; // the song file, from nuade_paula_padded_copy
    SsModule module;
    NuadePaulaChannel paula[SS_VOICES];
    SsVoice voices[SS_VOICES];
    uint8_t position;
    uint8_t row;
    uint8_t tick_counter; // ticks left of the row
};

// The eagleplayer's Check2 (the magic and the marker word 0x1190), plus everything the replayer needs from the
// song: the tables inside the file, every pattern the song plays, every sample a note starts. A song with a note the
// port does not model (see ss_module.c) is not accepted, so the reference player plays it.
bool ss_probe(const uint8_t* data, size_t size);

// The player keeps a copy of `data`.
SsPlayer* ss_create(const uint8_t* data, size_t size);
void ss_destroy(SsPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ss_start(SsPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
SsTickResult ss_tick(SsPlayer* player, NuadeVoices* voices);

// Ticks one row of the display grid spans.
uint32_t ss_grid_ticks(const SsPlayer* player);

SsVoiceState ss_voice_state(const SsPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SsPlayer* ss_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
