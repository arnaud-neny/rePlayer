// SPDX-License-Identifier: MIT
// Sound Programming Language player for the Amiga hunk executables that carry their replayer ("*.spl", prefix spl).

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

#define SPL_VOICES 4

// The CIA clock: the timebase of the tick length (the module header holds the CIA timer value).
#define SPL_TIMEBASE_HZ 709379

#define SPL_HISTORY 4

typedef struct SplPlayer SplPlayer;

typedef struct SplTickResult {
    bool looped; // every voice has jumped back in its stream since the last time (the eagleplayer's SongEnd)
} SplTickResult;

typedef struct SplRecord {
    uint32_t sample; // start of the sample
    uint16_t length; // words
    uint16_t octave; // Full, Lite: a word whose low byte counts; Tiny, Table: the value
    uint16_t flags;
    uint16_t period_speed;
    uint16_t decay_speed;
    uint16_t release_speed;
    uint16_t release_step;
    uint16_t volume;
    uint16_t transpose;
    uint16_t sample_number; // Table
    uint16_t loop_start;    // Table: bytes behind the sample's start
    uint16_t start_offset;  // Table: where in the sample the note starts
    uint16_t period_timer;
    uint16_t decay_timer;
    uint16_t release_timer;
    uint16_t current_volume;
    uint16_t release_volume;
    uint16_t history[SPL_HISTORY]; // the last periods, newest first
} SplRecord;

typedef struct SplVoice {
    uint32_t stream_at;
    uint32_t depth; // entries on the return stack
    uint8_t length_left;
    uint8_t length_reload;
    bool alive; // has not jumped back since the last SongEnd
    bool halted;
    SplRecord record;
    NuadePaulaChannel paula;
} SplVoice;

// The replayer comes in a few families. All of them read the same command stream of a voice: notes, lengths, calls
// and jumps. They differ in the instrument record, in what a note starts and in what runs between the notes.
typedef enum SplFamily {
    SplFamily_Full,  // record with period rotation, volume decay and a release that restarts the sample
    SplFamily_Lite,  // the same record; only the period swap of the effects exists, a note retriggers once
    SplFamily_Tiny,  // sixteen byte record, no effects
    SplFamily_Table, // sample numbers into a size table and an address table, a loop offset behind the start
} SplFamily;

// How often the command streams advance.
typedef enum SplDivider {
    SplDivider_Every,  // every tick (the CIA timer is the tempo)
    SplDivider_Swing,  // steps twice in `divider_reload + 1` ticks
    SplDivider_Tempo,  // the same with the reload chosen by the subsong
    SplDivider_Phased, // reload 6, and 7 on three of four rounds
    SplDivider_Bits,   // a 32 bit pattern selects the ticks that step
} SplDivider;

// What the init routine sets up for a subsong.
typedef struct SplSong {
    uint32_t streams[SPL_VOICES]; // where the voices' command streams start
    uint16_t tempo;               // SplDivider_Tempo: the reload of the divider
    bool sets_transpose;          // the init sets the transposition (else it stays at the build's initial value)
    uint8_t transpose;            // the initial transposition when `sets_transpose`
    uint8_t voices;               // voices that run; 0 means all four
} SplSong;

// What the init routine does to the hardware before the first tick.
typedef struct SplStartup {
    bool dma_on[SPL_VOICES]; // the voices whose DMA the init switches on
    bool plays_sample;       // voice 0 starts the sample at full volume
    uint32_t sample;         // its start (record 0 holds it too)
    uint32_t length_cell;    // the word that holds its length in bytes
    uint16_t record_sample;  // the start in record 0 when `plays_sample` is not set
    uint16_t record_octave;  // octave field of record 0 (a byte in the Tiny family, the high byte of a word otherwise)
} SplStartup;

// One build of the replayer.
typedef struct SplBuild {
    uint32_t load_base;  // where UADE's loader puts this module: the stacks hold addresses that the replayer reads back
    uint64_t hash;       // of the module bytes below `prefix_end` and of the relocations
    uint32_t prefix_end; // the replayer, its variables, the sequence data and the tables end here; samples follow
    SplFamily family;
    SplDivider divider;
    uint16_t divider_reload; // SplDivider_Swing
    uint32_t pattern;        // SplDivider_Bits: the pattern
    uint32_t period_table;   // 16 words
    uint32_t sample_base;    // added to the start of an instrument (Full family)
    uint8_t transpose;       // the initial transposition (the cell of the module)
    // Each voice's return stack: a run of longs in the module's own memory. In some builds they overlap tables and
    // samples.
    uint32_t stacks[SPL_VOICES];
    uint32_t size_table;       // Table family: a word for each sample, its size in bytes
    uint32_t address_table;    // Table family: a long for each sample, its address
    bool silent_middle_voices; // notes of voices 1 and 2 are dropped
    bool skip_fe;              // a 0xFE command byte is skipped like 0xF4
    bool stop_f5;              // 0xF5 stops the song, 0xF6 sets the transposition
    SplStartup startup;
    const SplSong* songs;
    uint32_t song_count;
} SplBuild;

typedef struct SplModule {
    uint8_t* chip;     // the code hunk with Paula's padding behind it; relocated to build->load_base
    uint8_t* original; // the hunk as loaded: the replayer writes into `chip` as it runs
    uint32_t size;     // bytes of the hunk
    const SplBuild* build;
    uint32_t subsong_count;
    uint32_t timer; // CIA timer value of the tick
} SplModule;

struct SplPlayer {
    SplModule* module;
    const SplSong* song;
    SplVoice voices[SPL_VOICES];
    uint32_t voice_count;
    uint8_t transpose;
    uint16_t divider;
    uint8_t phase;
    uint32_t ticks;
    bool playing;
    bool looped;
};

// True when the eagleplayer plays the file and this port reproduces it: the eagleplayer's checks and a replayer build
// that the port knows.
bool spl_probe(const uint8_t* data, size_t size);

// null when spl_probe is false.
SplPlayer* spl_create(const uint8_t* data, size_t size);
void spl_destroy(SplPlayer* player);

int spl_subsong_count(const SplPlayer* player);

// Length of a tick in 1 / SPL_TIMEBASE_HZ seconds: the CIA timer value of the module header.
uint32_t spl_tick_length(const SplPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool spl_start(SplPlayer* player, int subsong, NuadeVoices* voices);

SplTickResult spl_tick(SplPlayer* player, NuadeVoices* voices);

// The player's state behind a song it opened; 0 when the song is another player's.
SplPlayer* spl_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
