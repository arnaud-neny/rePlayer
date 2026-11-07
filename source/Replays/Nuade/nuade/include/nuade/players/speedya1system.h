// SPDX-License-Identifier: MIT
// Speedy A1 System player (the later music editor by Michael Winterberg), translated from the UADE eagleplayer
// "SpeedyA1System". A song is one file ("sas.<name>" or "<name>.sas"): a header, the order list, the patterns and a
// table of 32 IFF 8SVX samples. The eagleplayer holds the replayer, which is a different program from Speedy System's.

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

#define SA1_VOICES 4

// The CIA clock: the timebase of SA1_TICK_LENGTH.
#define SA1_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SA1_TIMEBASE_HZ seconds.
#define SA1_TICK_LENGTH 14187

#define SA1_SAMPLES 32

typedef struct Sa1Player Sa1Player;

typedef struct Sa1TickResult {
    bool looped; // the replayer reached the end of the song and restarted: the eagleplayer calls SongEnd
} Sa1TickResult;

typedef struct Sa1VoiceState {
    int note;       // semitones from C-0 (12 = C-1) of the last note started, -1 if none
    int instrument; // the sample number of that note, -1 if none
} Sa1VoiceState;

// A sample table entry as the replayer starts it. Offsets are in the file.
typedef struct Sa1Sample {
    uint32_t start;           // the data: AUDxLC
    bool envelope_valid;      // the start and the repeat lie in the file
    uint16_t start_len_words; // for a note with an envelope: one shot part and repeat part
    bool has_repeat;          // the repeat part has a length
    uint32_t repeat;          // where the repeat part starts: after the one shot part
    uint16_t repeat_len_words;
    bool plain_valid;
    uint16_t plain_len_words; // for a note without an envelope: the whole sample, played once
} Sa1Sample;

typedef struct Sa1Module {
    const uint8_t* data; // `size` bytes; the player's copy comes from nuade_paula_padded_copy
    size_t size;
    uint32_t speed; // ticks of a row - 1
    uint32_t start_position;
    uint32_t end_position; // the song ends when the position reaches it
    Sa1Sample samples[SA1_SAMPLES];
} Sa1Module;

typedef enum Sa1Envelope {
    Sa1Envelope_Off,
    Sa1Envelope_Attack,
    Sa1Envelope_Release,
} Sa1Envelope;

typedef enum Sa1Effect {
    Sa1Effect_None,
    Sa1Effect_PeriodAdd,
    Sa1Effect_PeriodSub,
    Sa1Effect_AttackVolume, // the volume becomes the attack delay of the last note with an envelope
} Sa1Effect;

typedef struct Sa1Voice {
    Sa1Envelope envelope;
    Sa1Effect effect;
    uint16_t effect_amount;
    bool in_sample;   // a note with an envelope started: the sample's repeat part is written every tick
    bool dma_pending; // the note starts when the row's cells are read
    unsigned sample;
    uint16_t period;
    uint16_t volume;
    uint16_t counter;
    uint16_t attack_delay;
    uint16_t release_delay;
    int note;
    int instrument;
} Sa1Voice;

struct Sa1Player {
    uint8_t* image; // the song file, from nuade_paula_padded_copy
    Sa1Module module;
    NuadePaulaChannel paula[SA1_VOICES];
    Sa1Voice voices[SA1_VOICES];
    uint32_t position;
    unsigned row;
    uint32_t tick_counter; // ticks left of the row
};

// The eagleplayer's Check2 (the header, the order list, the "FORM" that follows the patterns), plus everything the
// replayer needs from the song: the sample tables inside the file, every pattern cell the song plays, every sample a
// note starts. A song the port does not model is not accepted, so the reference player plays it.
bool sa1_probe(const uint8_t* data, size_t size);

// The player keeps a copy of `data`.
Sa1Player* sa1_create(const uint8_t* data, size_t size);
void sa1_destroy(Sa1Player* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool sa1_start(Sa1Player* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
Sa1TickResult sa1_tick(Sa1Player* player, NuadeVoices* voices);

// Ticks one row of the display grid spans.
uint32_t sa1_grid_ticks(const Sa1Player* player);

Sa1VoiceState sa1_voice_state(const Sa1Player* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
Sa1Player* sa1_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
