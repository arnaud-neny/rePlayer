// SPDX-License-Identifier: MIT
// onEscapee player (Akos Divianszky, 1997), translated from the UADE eagleplayer (Wanted Team).

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

#define ONESCAPEE_VOICES 4

// The CIA clock: the timebase of ONESCAPEE_TICK_LENGTH.
#define ONESCAPEE_TIMEBASE_HZ 709379
// One tick (DeliTracker's default 50 Hz CIA timer), in 1 / ONESCAPEE_TIMEBASE_HZ seconds. The player has no tick of its
// own: the tick is only where this port answers the audio interrupt.
#define ONESCAPEE_TICK_LENGTH 14187

// Bytes per buffer (the first move of InitSound, and the count AUDxLEN plays)
#define ON_CHUNK_BYTES 0x400

typedef struct OnEscapeePlayer OnEscapeePlayer;

typedef struct OnEscapeeTickResult {
    bool looped; // the replayer called SongEnd during this tick
} OnEscapeeTickResult;

typedef struct OnReplayer {
    const uint8_t* module; // the module bytes (the player owns them)
    uint32_t module_size;  // size with the two low bits cleared, as InitPlayer stores it
    uint32_t pos;          // the next module byte to decode
    uint32_t remaining;    // module bytes not decoded yet
    int8_t buffers[2][ON_CHUNK_BYTES];
    int fill;            // the buffer the next refill writes; the other one is the one AUDxLC points at
    bool looped;         // SongEnd was called during this tick
    uint32_t wraps_seen; // buffer starts of voice 0 already answered
    NuadePaulaChannel paula[ONESCAPEE_VOICES];
} OnReplayer;

struct OnEscapeePlayer {
    uint8_t* data; // owned copy of the module
    OnReplayer r;
};

// The eagleplayer's module check (a run of $AA55FF00 longwords, see onescapee.c) plus the size the replayer needs.
bool onescapee_probe(const uint8_t* data, size_t size);

// 0 when the data is not an onEscapee module (or out of memory).
OnEscapeePlayer* onescapee_create(const uint8_t* data, size_t size);
void onescapee_destroy(OnEscapeePlayer* player);

// (Re)start the song. The player drives `voices` (Paula channel n = voice n).
bool onescapee_start(OnEscapeePlayer* player, NuadeVoices* voices);

OnEscapeeTickResult onescapee_tick(OnEscapeePlayer* player, NuadeVoices* voices);

// The player's state behind a song it opened; 0 when the song is another player's.
OnEscapeePlayer* onescapee_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
