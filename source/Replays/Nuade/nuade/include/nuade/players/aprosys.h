// SPDX-License-Identifier: MIT
// AProSys player (Petter A. Urkedal, 1989/1990), translated from the UADE eagleplayer "AProSys" (adapted by
// Delirium & Buggs / DFT).

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

#define APS_VOICES 4

// The CIA clock: the timebase of APS_TICK_LENGTH.
#define APS_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / APS_TIMEBASE_HZ seconds.
#define APS_TICK_LENGTH 14187

#define APS_ENVELOPES 4

typedef struct ApsPlayer ApsPlayer;

typedef struct ApsTickResult {
    bool looped; // the order list took a backward jump (the unconditional one: the song's loop)
    bool ended;  // the order list reached its end command: the player stops the audio
} ApsTickResult;

// What voice (0-based) last started: the note in semitones from C-0 (-1 if none yet) and the instrument (-1 if none).
typedef struct ApsVoiceState {
    int note;
    int instrument;
} ApsVoiceState;

typedef struct ApsModule {
    const uint8_t* data;
    uint32_t size;
    uint32_t order;
    uint32_t steps;
    uint32_t tracks;
    uint32_t arpeggios;
    uint32_t instruments;
    uint32_t envelopes;
    uint32_t sample_records;
    uint32_t sample_data;
    const int8_t* wave;    // the player's wave memory, read through the same addresses as the file
    uint32_t wave_address; // where it lies
} ApsModule;

typedef struct ApsWaveFilter {
    int16_t first;  // memory of the first filter form
    int16_t second; // and of the second
} ApsWaveFilter;

typedef struct ApsEnvelope {
    uint32_t stream;    // the current (duration, target) pair
    uint16_t remaining; // ticks left in the current pair
    int16_t level;      // 8.8 fixed point, moves by `delta` every tick
    int16_t delta;
} ApsEnvelope;

typedef struct ApsVoice {
    uint8_t note;        // index into the period table
    uint32_t instrument; // address of the instrument record
    uint8_t volume;
    uint32_t arpeggio; // address in the arpeggio stream (arpeggio mode)
    uint8_t slide;     // glide speed (period steps per tick; 0 = jump)
    uint32_t sample;   // address of the sample record
    ApsEnvelope env[APS_ENVELOPES];
    uint8_t channel_volume; // the cell volume column, slid towards its target
    uint16_t period;
    uint32_t track; // address of the current track entry
    bool note_on;   // a note started and was not released
    bool fresh;     // a note started since the last wave build
    bool kick;      // a chained sound waits for its first audio interrupt
    ApsWaveFilter filter;
    uint32_t wave_build; // the wave buffer to build next, an offset in the wave memory
    uint32_t wave_spare; // and the other
    bool irq_enabled;    // INTENA
    bool irq_pending;    // INTREQ
    uint32_t wraps_seen; // the mixer's block starts already answered
    int shown_note;
    int shown_instrument;

    NuadePaulaChannel paula;
} ApsVoice;

struct ApsPlayer {
    ApsModule module;
    uint8_t* data; // owned copy of the module, with the Paula tail (nuade_paula_padded_copy)

    uint8_t order_pos;
    uint8_t steps_done;  // steps played of the current "steps" entry
    uint8_t row;         // row of the current step
    uint32_t step;       // address of the current step
    uint8_t ticks_left;  // of the current row
    uint8_t row_ticks;   // the row length
    uint8_t speed;       // ticks per duration unit
    uint8_t repeat_left; // passes left of a counted repeat entry; APS_NO_REPEAT when none runs
    uint8_t jump_budget;
    uint8_t period_base; // period table position of note 0
    bool looped;
    bool ended;

    int8_t* chip; // the player's chip allocations: the silent block, the wave memory at APS_SILENCE_DISTANCE, then the
                  // Paula tail (nuade_paula_padded_copy of APS_CHIP_BYTES)
    int8_t* wave; // APS_WAVE_BYTES of wave memory, in `chip`
    ApsVoice voices[APS_VOICES];
};

// The eagleplayer's module check and InitPlayer's split: the "ADRVPACK" magic, then nine length-prefixed sections that
// all fit in the file.
bool aps_probe(const uint8_t* data, size_t size);

// 0 when the data is not an AProSys module (or out of memory).
ApsPlayer* aps_create(const uint8_t* data, size_t size);
void aps_destroy(ApsPlayer* player);

// (Re)start the song from its first order entry; the player drives `voices` (Paula channel n = voice n).
bool aps_start(ApsPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
ApsTickResult aps_tick(ApsPlayer* player, NuadeVoices* voices);
ApsVoiceState aps_voice_state(const ApsPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
ApsPlayer* aps_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
