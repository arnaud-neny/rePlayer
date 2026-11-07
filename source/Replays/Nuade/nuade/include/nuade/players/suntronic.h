// SPDX-License-Identifier: MIT
// SUN-Tronic player (Sun-Tronic of TSM, 1997), translated from the UADE eagleplayer "SUN-Tronic". The eagleplayer
// carries no replayer: every module is a 68k program that the eagleplayer relocates, patches and calls once per tick.
// The replayer is a wavetable synthesizer in three generations; this port models the builds listed in
// players/suntronic/sun_module.c and refuses every other module, which UADE then plays.

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

#define SUN_VOICES 4

// The eagleplayer's file name prefix (the files are also named with it as a suffix).
#define SUN_EXTENSION "sun"

// The CIA clock: the timebase of SUN_TICK_LENGTH.
#define SUN_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / SUN_TIMEBASE_HZ seconds.
#define SUN_TICK_LENGTH 14187

// The most bytes of a synth waveform (generation A: up to 0x40 words); generations B and C use 32.
#define SUN_WAVE_BYTES 0x80

typedef struct SunPlayer SunPlayer;

typedef enum SunStatus {
    SunStatus_Ok = 0,
    SunStatus_InvalidData,
    SunStatus_OutOfMemory,
} SunStatus;

typedef struct SunCreateResult {
    SunPlayer* player; // 0 on failure
    SunStatus status;
} SunCreateResult;

typedef struct SunTickResult {
    bool looped; // the eagleplayer's SongEnd call: the step table wrapped to its first step
} SunTickResult;

// What voice (0-based) plays: the period (0 if none yet) and the synth instrument of its last instrument command.
typedef struct SunVoiceState {
    uint32_t period;
    int instrument;
} SunVoiceState;

// A replayer generation. They differ in the voice record, the instrument record and the song's step table.
typedef enum SunGen {
    SunGen_A, // eagleplayer id 0x18: master volume and fade, morph types, 0x130 byte voice record
    SunGen_B, // id 0x10, step entries of 20 bytes: vibrato, 0x68 byte voice record
    SunGen_C, // id 0x10, step entries of 16 bytes: no vibrato, 0x62 byte voice record
} SunGen;

// One build of the replayer, found by the hash of the module's code. Offsets are from the variable block that follows
// the code (the register A6 of the replayer).
typedef struct SunBuild {
    uint64_t code_hash; // FNV-1a over the code, which pins every operand the replayer addresses its variables with
    uint32_t vars;      // size of the code = offset of the variable block
    SunGen gen;
    uint32_t period_table;
    uint32_t adjust_table;
    uint32_t synth_table;
    uint32_t sample_table;
    uint32_t step_table;
    uint32_t master_volume; // A: the variable the master volume is read from
    // A: the master volume is the module's byte 0x18 of the variables, which no command changes (the build addresses
    // it relative to the program counter and misses its variable by one). The fade then cannot be modelled.
    bool fixed_master_volume;
    // A: the filter morph (type 4) scales by this; a build without the term has 0.
    uint16_t filter_scale;
} SunBuild;

// The module's memory: the relocated image, then zeros (UADE's chip RAM above the module).
typedef struct SunMemory {
    uint8_t* bytes; // size bytes of module, then zeros
    uint32_t size;
} SunMemory;

// A synth instrument as the module stores it (the record size and field offsets differ per generation).
typedef struct SunInstrument {
    uint32_t envelope;     // volume envelope bytes
    uint16_t envelope_end; // the position that loops back to envelope_loop
    uint16_t envelope_loop;
    uint16_t envelope_stop; // C: the end of the looping part (envelope_end ends the attack)
    uint32_t vibrato;       // A, B: pitch modulation bytes
    uint16_t vibrato_end;
    uint16_t vibrato_loop;
    int16_t vibrato_step; // added to the vibrato phase every tick
    uint32_t morph;       // waveform morph bytes
    uint16_t morph_end;
    uint16_t morph_loop;
    uint16_t morph_stop; // C: the end of the looping part
    uint32_t wave_a;
    uint32_t wave_b;    // 0 = none
    uint8_t words;      // waveform length in words
    uint8_t morph_type; // A
} SunInstrument;

typedef struct SunModule {
    SunMemory memory;
    const SunBuild* build;
    uint32_t vars;        // address of the variable block
    uint32_t synth_count; // records in the synth instrument table
    uint32_t step_count;  // steps before the zero that ends the table
} SunModule;

typedef struct SunVoice {
    uint32_t pattern;        // address of the next pattern item
    bool active;             // an instrument has started
    SunInstrument inst;      // the instrument it plays
    int instrument;          // its index in the synth table, -1 before the first
    uint8_t last_instrument; // A: the last instrument byte; a note restarts it
    int8_t transpose;        // A, B: added to the notes of the step

    uint16_t pitch;      // note in the high byte, fraction below
    int16_t pitch_slide; // added to the pitch every tick
    uint8_t volume;      // 0 .. 0x80 (A, B) or 0x40 (C)
    int8_t volume_slide;
    uint8_t period_row; // selects a row of the adjust table; the column counts through 16 (A) or 8 ticks
    uint8_t period_column;
    uint16_t envelope_pos;
    uint16_t morph_pos;
    uint16_t vibrato_phase;
    uint16_t vibrato_pos;
    bool primed;           // A, B: the back buffer holds a waveform the morph continues from
    bool envelope_looping; // C: the envelope is in its loop part
    bool morph_looping;    // C: the morph is in its loop part

    uint8_t out_volume;
    uint16_t period;

    uint16_t restart_words; // nonzero when the voice restarts its DMA: the waveform length in words
    uint16_t latched_words; // A: restart_words as stage 1 saw it
    bool start_dma;         // A: stage 1 triggered this voice

    NuadePaulaChannel paula;
    int8_t wave[2][SUN_WAVE_BYTES]; // [0] plays, [1] is written by the morph
} SunVoice;

struct SunPlayer {
    SunModule module;
    SunVoice voices[SUN_VOICES];

    uint8_t tick_count;
    uint8_t row_count;
    uint8_t speed;
    uint16_t step_index;
    uint8_t master_volume; // A
    int8_t fade_step;      // A: added to the master volume every fade_reload + 1 ticks
    uint8_t fade_reload;
    int8_t fade_timer;
    uint16_t seed[2]; // A: random morph
};

// True when the eagleplayer plays the module and this port reproduces it: the eagleplayer's check, a replayer build the
// port knows, and sequence data the port models.
bool sun_probe(const uint8_t* data, size_t size);

SunCreateResult sun_create(const uint8_t* data, size_t size);
void sun_destroy(SunPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool sun_start(SunPlayer* player, NuadeVoices* voices);

SunTickResult sun_tick(SunPlayer* player, NuadeVoices* voices);

// "SUN-Tronic".
const char* sun_format_name(const SunPlayer* player);
SunVoiceState sun_voice_state(const SunPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SunPlayer* sun_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
