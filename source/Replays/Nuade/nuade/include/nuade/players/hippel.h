// SPDX-License-Identifier: MIT
// Jochen Hippel COSO player, translated from the replayer in UADE's "hipc player module V0.2" (Hippel's code, adapted
// for UADE by shd) and, as a variant, from Wanted Team's "Jochen Hippel COSO player module V1.0" (prefix "soc").

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

#define HIP_VOICES 4

// The CIA clock: the timebase of HIP_TICK_LENGTH.
#define HIP_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / HIP_TIMEBASE_HZ seconds.
#define HIP_TICK_LENGTH 14187

typedef struct HipPlayer HipPlayer;

typedef struct HipTickResult {
    bool looped; // a voice's track wrapped to the subsong start during this tick
    bool ended;  // the song stopped itself (track command 8): silence from now on
} HipTickResult;

typedef struct HipVoiceState {
    int note;       // period table index of the note last started, -1 if none
    int instrument; // volume sequence (instrument) number, -1 if none
} HipVoiceState;

// What differs between the replayer of UADE's "hipc" player and the one of Wanted Team's "soc" player.
typedef struct HipVariant {
    bool song_end_on_first_voice; // the song loops when voice 0 wraps (else any voice)
    bool subsong_fixups;          // a subsong speed of 0 is 4; a subsong whose last position precedes its first is 0..0
    bool track_speed;             // track command E sets the speed
    bool extended_sequences;      // fseq EA (random volume) and values above it; vseq commands E9..EF skip
    bool wanted_team_module;      // the player's module checks (Check2, sample size, subsong count in the header)
} HipVariant;

typedef struct HipModule {
    const HipVariant* variant;
    uint8_t* data; // owned, writable: sample-pack starts are patched in place; NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes
                   // follow the module (nuade_paula_padded_copy)
    uint32_t size; // the module's size
    uint32_t fseqs;
    uint32_t vseqs;
    uint32_t patterns;
    uint32_t tracks;
    uint32_t subsongs;
    uint32_t samples;
    uint32_t sample_data;
    int subsong_count;
} HipModule;

// A sequence: in the module, or the replayer's start sequence.
typedef struct HipSequence {
    const uint8_t* data;
    uint32_t size;
} HipSequence;

typedef struct HipVibrato {
    uint8_t speed;
    uint8_t depth;
    uint8_t delay; // ticks before the vibrato starts
    uint8_t value; // 0..depth
    bool rising;   // the value moves up to the depth, else down to 0
} HipVibrato;

typedef struct HipVoice {
    int index;
    uint16_t dma_bit;
    uint32_t track;     // module offset of this voice's entry in the first position of the subsong
    uint32_t track_end; // ... in the position after the last one
    uint16_t track_offset;
    uint32_t pattern; // module offset of the next pattern byte, or PATTERN_NONE (read the next track entry)
    int8_t row_counter;
    uint8_t row_speed;

    uint8_t note;
    uint8_t info;  // instrument (bits 0-4), portamento (5), alternative frequency sequence (6)
    uint8_t param; // portamento speed or alternative frequency sequence
    uint8_t instrument_transpose;
    uint8_t note_transpose;
    uint8_t channel_volume; // 0..100
    uint16_t porta;

    HipSequence fseq;
    uint16_t fseq_pos;
    uint8_t fseq_wait;
    uint8_t fseq_value; // note offset, or an absolute note with bit 7 set

    HipSequence vseq;
    uint16_t vseq_pos;
    uint8_t vseq_counter;
    uint8_t vseq_speed;
    uint8_t vseq_wait;
    uint8_t volume;
    bool seq_period; // this tick's base period is seq_period_word, not the note's
    uint16_t seq_period_word;
    uint8_t random_volume; // fseq EA: the depth of the next random volume cut, 0 = none

    HipVibrato vibrato;

    uint8_t sample_number; // 0xFF = none / forced restart
    const int8_t* loop;    // AUDxLC for the repeat
    uint16_t loop_length;  // words

    bool sweep_active;
    bool sweep_stopped;
    bool sweep_started;
    uint32_t sweep_start; // sample data offsets
    uint32_t sweep_end;
    uint16_t sweep_pos; // words
    int16_t sweep_delta;
    uint16_t sweep_length;
    int8_t sweep_counter;
    uint8_t sweep_speed;

    uint16_t period;
    int last_instrument;

    NuadePaulaChannel paula;
} HipVoice;

typedef struct HipSong {
    uint16_t speed;
    uint16_t speed_counter;
    bool stopped;
    bool crashed;      // the replayer left its code: UADE's emulation stops with the tick it happened in
    uint16_t dma_mask; // channels restarted in this tick
    uint16_t random_seed;
    bool looped;
    HipVoice voices[HIP_VOICES];
} HipSong;

struct HipPlayer {
    HipModule module;
    HipSong song;
};

// "COSO" at 0 and "TFMX" at 0x20, as the eagleplayer checks.
bool hip_probe(const uint8_t* data, size_t size);

// 0 when the data is not a COSO module (or out of memory).
HipPlayer* hip_create(const uint8_t* data, size_t size);

// The Wanted Team player (files named "soc.name" or "name.soc"): the module has to pass its Check2, which means that
// its samples are in the module. The Atari ST modules (sample set file SMP.set) are not taken: UADE plays them reading
// memory the port does not have. 0 for those and for data that is not a COSO module.
HipPlayer* hip_create_wanted_team(const uint8_t* data, size_t size);

void hip_destroy(HipPlayer* player);

int hip_subsong_count(const HipPlayer* player);

// (Re)start subsong `subsong` (0-based; the replayer numbers them from 1). The player drives `voices` (Paula channel
// n = voice n).
bool hip_start(HipPlayer* player, int subsong, NuadeVoices* voices);

HipTickResult hip_tick(HipPlayer* player, NuadeVoices* voices);

// Ticks per sequencer step of the running subsong.
int hip_speed(const HipPlayer* player);

HipVoiceState hip_voice_state(const HipPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
HipPlayer* hip_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
