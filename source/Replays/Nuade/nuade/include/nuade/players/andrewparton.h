// SPDX-License-Identifier: MIT
// Andrew Parton player (the music of the game "B-17 Flying Fortress", 1992), translated from the UADE eagleplayer
// "Andrew Parton player module V1.1" (Wanted Team). A module is a "BANK" file: an instrument table (sample, loop and
// envelope parameters), the sample data and a MIDI-like event stream; the eagleplayer holds the replayer.

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

#define AP_VOICES 4

// The CIA clock: the timebase of AP_TICK_LENGTH.
#define AP_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / AP_TIMEBASE_HZ seconds.
#define AP_TICK_LENGTH 14187

#define AP_INSTRUMENT_COUNT 20
#define AP_CHANNEL_COUNT 16 // MIDI-like channels; channel n plays on voice n % AP_VOICES

typedef struct ApPlayer ApPlayer;

typedef struct ApTickResult {
    bool ended; // the song is over: the end event ran (the eagleplayer's SongEnd) or the stream ran out
} ApTickResult;

typedef struct ApVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // instrument (0..31) of the last note started, -1 if none
} ApVoiceState;

typedef struct ApEnvelope {
    uint8_t attack; // rates per tick, in levels
    uint8_t decay;
    uint8_t sustain; // level; a voice holds it scaled by the velocity of the note
    uint8_t release;
} ApEnvelope;

typedef struct ApVibrato {
    uint8_t delay; // ticks until it starts
    uint8_t depth; // period steps per vibrato unit
    uint8_t speed; // steps through the vibrato table per tick
} ApVibrato;

typedef struct ApInstrument {
    bool has_sample;
    uint32_t sample;      // offset of the sample data in the module
    uint32_t length;      // bytes
    uint32_t loop_length; // bytes; 0 plays the sample once, then silence
    uint32_t loop_start;  // offset in the sample
    ApEnvelope envelope;
    ApVibrato vibrato;
} ApInstrument;

typedef struct ApModule {
    uint8_t* data; // the file and NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes (nuade_paula_padded_copy)
    uint32_t size; // the file's size
    ApInstrument instruments[AP_INSTRUMENT_COUNT];
    uint32_t stream; // offset of the event stream
} ApModule;

typedef enum ApPhase {
    ApPhase_Attack = 0,
    ApPhase_Decay = 1,
    ApPhase_Sustain = 2,
    ApPhase_Release = 100,
    ApPhase_Off = 0xFF,
} ApPhase;

typedef struct ApVoice {
    NuadePaulaChannel paula;
    ApEnvelope envelope; // the instrument's, with the sustain scaled by the velocity (a signed level)
    ApPhase phase;
    int8_t level;
    int8_t peak; // the level the attack rises to: half the velocity
    uint16_t period;
    ApVibrato vibrato; // the delay counts down
    uint8_t vibrato_index;
    int note; // display only
    int instrument;
} ApVoice;

typedef struct ApSequencer {
    bool running;
    uint32_t position;
    uint32_t loop_position;
    uint8_t wait;                       // ticks to the next events
    uint8_t status;                     // running status
    uint8_t programs[AP_CHANNEL_COUNT]; // instrument per channel
} ApSequencer;

struct ApPlayer {
    ApModule module;
    ApSequencer sequencer;
    ApVoice voices[AP_VOICES];
};

// The eagleplayer's Check2: "BANK", then 20 longs below 0x200000 and 40 longs below 0x10000.
bool ap_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer); null if it is not a valid module.
ApPlayer* ap_create(const uint8_t* data, size_t size);
void ap_destroy(ApPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ap_start(ApPlayer* player, NuadeVoices* voices);

ApTickResult ap_tick(ApPlayer* player, NuadeVoices* voices);

ApVoiceState ap_voice_state(const ApPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
ApPlayer* ap_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
