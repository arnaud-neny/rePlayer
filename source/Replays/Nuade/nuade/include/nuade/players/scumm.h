// SPDX-License-Identifier: MIT
// SCUMM sound drivers (Lucasfilm Games: Zak McKracken, Maniac Mansion), translated from the replayers inside the
// modules and the UADE eagleplayer "SCUMM". A module is the driver's code, called once per tick, with the song's
// events, instrument records, envelopes and samples behind it. The driver was rewritten for each tune: this port knows
// the eight versions of the corpus (two sweep effects, a two-channel effect driver in three variants, a four-channel
// music driver in two) and refuses every other file.

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

#define SCUMM_VOICES 4

// The CIA clock: the timebase of SCUMM_TICK_LENGTH.
#define SCUMM_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SCUMM_TIMEBASE_HZ seconds.
#define SCUMM_TICK_LENGTH 14187

#define SCM_TRACKS 4
#define SCM_MAX_CHANNELS 4
// The channel byte holds the instrument number and is tested for bit 7 (free).
#define SCM_MAX_INSTRUMENTS 0x80u
// The alarm: four bouncing periods. The pairs (period, step) lie in a word array that the tick also reads as four
// periods, one per channel.
#define SCM_ALARM_PAIRS 4

typedef struct ScummPlayer ScummPlayer;

typedef struct ScummVoiceState {
    int period;     // AUDxPER as last written, 0 while the voice's DMA is off
    int instrument; // the instrument of the last note started on the voice, -1 if none
} ScummVoiceState;

typedef enum ScmDriver {
    ScmDriver_Alarm,        // four voices with periods that bounce between two limits
    ScmDriver_TeleportDown, // one voice at a time, the period falls
    ScmDriver_TeleportUp,   // ... the period rises
    ScmDriver_Duo,          // event tracks on two voices (Paula channels 2 and 3), notes fade out through the envelope
    ScmDriver_Quad,         // event tracks on four voices, a note ends after its duration
} ScmDriver;

// What the driver does when the first track reaches its last event (the other tracks start over).
typedef enum ScmTrackEnd {
    ScmTrackEnd_Restart,       // start the track over and go on with the tick
    ScmTrackEnd_RestartReturn, // start it over and end the tick
    ScmTrackEnd_Freeze,        // end the tick, again every tick: nothing moves any more
} ScmTrackEnd;

// The immediates of the code that place the song data.
typedef enum ScmOperand {
    ScmOperand_Records,   // instrument records, from the entry address
    ScmOperand_Envelopes, // envelope tables, from the entry address
    ScmOperand_Events0,   // the first track's events, from the entry address
    ScmOperand_Events1,   // each later track's events, from the previous track's
    ScmOperand_Events2,
    ScmOperand_Events3,
    ScmOperand_ZeroWrite, // a displacement: the long it addresses is cleared at every note start
    ScmOperand_Count
} ScmOperand;

// What the two tracker engines differ in (one entry per engine, see scm_engine).
typedef struct ScmEngine {
    bool tracker;         // event tracks (otherwise a sweep effect)
    uint32_t channels;    // driver channels
    uint32_t instruments; // instrument numbers the load-time proof accepts
    uint32_t first_voice; // the Paula channel of driver channel 0
    bool two_channel; // the Duo behaviours: fade-out release, DMA off at stop and init (Quad: cut, DMA on, other tick
                      // rules)
} ScmEngine;

typedef struct ScmVersion {
    const char* name;
    ScmDriver driver;
    bool via_branch;    // the entry at offset 4 is a bra.w to the code
    uint16_t code_len;  // from the code's first byte
    uint64_t code_hash; // the code with its operands zeroed
    ScmTrackEnd first_track_end;
    bool stops_dma_on_start;               // a note start switches the channel's DMA off first
    uint8_t operand_bytes;                 // 4: adda.l immediates, 2: adda.w
    uint16_t operand_at[ScmOperand_Count]; // offsets from the code's first byte, 0: the version has none
} ScmVersion;

typedef struct ScmModule {
    const ScmVersion* version;
    const ScmEngine* engine;
    uint8_t* image; // nuade_paula_padded_copy of the module: the driver patches its samples
    uint32_t size;
    uint32_t entry;   // the code's first byte
    uint32_t samples; // the sample base
    uint32_t records;
    uint32_t envelopes;
    uint32_t events[SCM_TRACKS];
    int16_t zero_write; // displacement from the note's sample start
} ScmModule;

typedef struct ScmBounce {
    uint16_t period;
    uint16_t step;
} ScmBounce;

typedef struct ScmTeleport {
    uint16_t cycle; // the voice that plays this tick
    uint16_t base;
} ScmTeleport;

// The working fields of an instrument record.
typedef struct ScmNote {
    uint16_t volume_position; // 0: the note has not been updated yet
    int16_t duration;         // ticks left
    bool releasing;           // the two-channel driver: the note ran out its duration
    uint16_t pitch_offset;
    uint16_t pitch_position;
    uint8_t channel;
} ScmNote;

typedef struct ScmTrack {
    uint16_t position;
    uint16_t timer;
} ScmTrack;

struct ScummPlayer {
    ScmModule module;
    uint8_t* pristine; // the module as loaded: the drivers patch samples
    bool first_tick;
    ScmBounce alarm[SCM_ALARM_PAIRS];
    ScmTeleport teleport;
    ScmTrack tracks[SCM_TRACKS];
    uint8_t playing[SCM_MAX_CHANNELS]; // the instrument on each channel, SCM_CHANNEL_FREE for none
    ScmNote notes[SCM_MAX_INSTRUMENTS];
    int last_instrument[SCUMM_VOICES];
    NuadePaulaChannel paula[SCUMM_VOICES];
};

// The eagleplayer's Check2 plus the limits of what this port models: the driver code must be one of the known
// versions and every module byte the driver can read or write must be reproducible.
bool scumm_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `scumm_probe` rejects it.
ScummPlayer* scumm_create(const uint8_t* data, size_t size);
void scumm_destroy(ScummPlayer* player);

// (Re)start the song from its beginning. Voice n of `scumm_tick` is Paula channel n.
bool scumm_start(ScummPlayer* player);

// Run one player tick and hand the Paula registers it wrote to the voices. The eagleplayer never signals the end of
// the song, so a tick has no result.
void scumm_tick(ScummPlayer* player, NuadeVoices* voices);

ScummVoiceState scumm_voice_state(const ScummPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
ScummPlayer* scumm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
