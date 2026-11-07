// SPDX-License-Identifier: MIT
// Darius Zendeh player ("Mark II" replayer; the known modules are the music of R-Type, Katakis, Crash and Burn and a
// cracktro), translated from the UADE eagleplayer "Darius Zendeh" (Wanted Team). The replayer is in every module: a
// small 68k routine that steps through a list of (pattern, transpose) pairs and plays the rows of those patterns with
// one instrument per voice and row. The eagleplayer checks which of four code versions a module has and calls the code;
// the player finds the same tables in the code and runs the replayer logic in C.

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

#define DZ_VOICES 4

// The CIA clock: the timebase of DZ_TICK_LENGTH.
#define DZ_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / DZ_TIMEBASE_HZ seconds.
#define DZ_TICK_LENGTH 14187

// A row is set up on every second tick; the tick between switches off the voices whose row asks for it.
#define DZ_TICKS_PER_ROW 2

// What the init code sets up for one subsong
#define DZ_MAX_SONGS 32

typedef struct DzPlayer DzPlayer;

typedef struct DzTickResult {
    bool looped; // the song list ran out (the replayer's end flag)
    bool ended;  // the song stopped for good: a one-shot song ran out
} DzTickResult;

typedef struct DzVoiceState {
    int note;       // semitone of the period the voice's last row played, -1 if none
    int instrument; // instrument number of the last row, -1 if none
} DzVoiceState;

typedef struct DzVoice {
    NuadePaulaChannel paula;
    uint16_t last_period; // slides start from it
    bool stops;           // the stop tick after the row switches the voice's DMA off
    int note;
    int instrument;
} DzVoice;

typedef struct DzModule {
    uint8_t* data;
    uint32_t size;
} DzModule;

// The code versions: the type character the eagleplayer's Check2 gives a module.
typedef enum DzVersion {
    DzVersion_S, // a single song, list and patterns at fixed places (Katakis)
    DzVersion_1, // several songs, set up by the init code (R-Type levels)
    DzVersion_2, // a single song, chosen by the init command (R-Type jingles, Crash and Burn)
    DzVersion_3, // accepted by the eagleplayer's check; no module of it is known, the locator rejects it
} DzVersion;

typedef struct DzSong {
    uint32_t start_list;   // the list of (pattern, transpose) steps a two voice song starts from
    uint32_t loop_list;    // the list a four voice song starts from, and every song continues with after its end
    uint32_t pattern_base; // pattern 0, the first record
    uint32_t record_size;  // bytes of one pattern
    bool one_shot;         // the row routine does not continue with the loop list at the end of the list
    bool ends_at_list_end; // the eagleplayer stops the song for good when its list ran out
    int voices;            // 4, or 2 (two voice songs step 4 bytes of a list entry)
} DzSong;

// Everything the replayer reads from the code
typedef struct DzLayout {
    DzVersion version;

    uint32_t instrument_table;  // entries: sample offset, length in words
    bool wide_instruments;      // 8 byte entries (long offset, an unused word, length); version S: word offset, length
    uint32_t sample_base;       // the sample offsets count from here
    uint32_t period_table;      // words, indexed by note + transpose
    uint32_t last_period_table; // words, one per voice: the period of the last row (slides start from it)
    bool slide;                 // a note with bit 7 set slides from the last period
    bool stop_on_zero_flags;    // the DMA stop flag is "flags byte is 0", otherwise "bit 0 of the flags byte is clear"
    uint32_t phase;             // the word that alternates between the row tick and the stop tick
    bool silent_init;           // the init ends with the shutdown routine: DMA off, volumes 0

    int song_count;
    DzSong songs[DZ_MAX_SONGS];
} DzLayout;

struct DzPlayer {
    DzModule module; // the replayer only reads it
    DzLayout layout;

    DzSong song;
    DzVoice voices[DZ_VOICES];
    uint32_t list_pos; // the list entry of the voice 0, or the next step's after a row
    uint32_t row_pos;  // the current row in pattern 0
    bool row_tick;     // the next tick sets a row; otherwise it is the stop tick
    bool end_flag;     // the list ran out once (the replayer's end flag, no song clears it)
    bool stopped;      // the eagleplayer stopped the song for good
};

// The eagleplayer's Check2 (the code starts like a known replayer version) and the code is a variant whose tables
// the player can find, so that a file the player cannot play falls back to the uade player.
bool dz_probe(const uint8_t* data, size_t size);

// Copies the module and finds the replayer's tables in its code; 0 when the code is not a known variant.
DzPlayer* dz_create(const uint8_t* data, size_t size);
void dz_destroy(DzPlayer* player);

// The eagleplayer's type character: 'S' (a single song), '1' (several songs, 2 or 4 voices), '2' (a single song).
char dz_version(const DzPlayer* player);
int dz_subsong_count(const DzPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool dz_start(DzPlayer* player, int subsong, NuadeVoices* voices);

DzTickResult dz_tick(DzPlayer* player, NuadeVoices* voices);

DzVoiceState dz_voice_state(const DzPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
DzPlayer* dz_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
