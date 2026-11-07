// SPDX-License-Identifier: MIT
// NTSP-system player (Skjeggspir & Nightraver of Contraz), translated from the UADE eagleplayer (Wanted Team).
// A module is a stream of packed samples that the player unpacks into two chip buffers and plays on all four Paula
// channels.

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

#define NTSP_VOICES 4

// The CIA clock: the timebase of NTSP_TICK_LENGTH.
#define NTSP_TIMEBASE_HZ 709379
// The replayer has no tick of its own, only the channel 0 audio interrupt. The host ticks at 50 Hz and the player
// answers the interrupts that fired in between (a buffer plays for 82 ms).
#define NTSP_TICK_LENGTH 14187

#define NTSP_BUFFER_BYTES 0x708u

typedef struct NtspPlayer NtspPlayer;

typedef struct NtspTickResult {
    bool looped; // the replayer called SongEnd (the stream ran out) since the last tick
} NtspTickResult;

typedef struct NtspModule {
    const uint8_t* stream; // the packed blocks
    uint32_t samples;      // header: bytes the stream unpacks to
} NtspModule;

typedef struct NtspStream {
    size_t pos;         // next packed byte
    uint32_t remaining; // samples still to unpack
} NtspStream;

struct NtspPlayer {
    uint8_t* data; // the module
    NtspModule module;

    NtspStream stream;
    int8_t buffers[2][NTSP_BUFFER_BYTES]; // the replayer's bss: both buffers start as silence
    uint32_t free_buffer;                 // the buffer the interrupt unpacks into next (the one not playing)
    uint32_t wraps_seen;                  // buffer starts of channel 0 already answered
    bool looped;                          // SongEnd was called since the last tick

    NuadePaulaChannel paula[NTSP_VOICES];
};

// The eagleplayer's module check plus the init's size check and the stream bounds the replayer would read.
bool ntsp_probe(const uint8_t* data, size_t size);

// 0 when the data is not an NTSP-system module (or out of memory).
NtspPlayer* ntsp_create(const uint8_t* data, size_t size);
void ntsp_destroy(NtspPlayer* player);

// (Re)start the song. The player drives `voices` (Paula channel n = voice n).
bool ntsp_start(NtspPlayer* player, NuadeVoices* voices);

NtspTickResult ntsp_tick(NtspPlayer* player, NuadeVoices* voices);

// The player's state behind a song it opened; 0 when the song is another player's.
NtspPlayer* ntsp_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
