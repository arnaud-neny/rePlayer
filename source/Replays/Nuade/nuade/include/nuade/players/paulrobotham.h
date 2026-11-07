// SPDX-License-Identifier: MIT
// Paul Robotham player, translated from the UADE eagleplayer "PaulRobotham" (Wanted Team). A song is a "name.dat"
// file (four voice streams, vibrato and envelope tables, instrument records) with its samples in "name.ssd" or, if
// that is missing, "mdtest.ssd" in the same directory; the replayer is in the eagleplayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PR_VOICES 4

// The CIA clock: the timebase of PR_TICK_LENGTH.
#define PR_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / PR_TIMEBASE_HZ seconds.
#define PR_TICK_LENGTH 14187

#define PR_LOOP_DEPTH 20 // nested loops of one voice

typedef struct PrPlayer PrPlayer;

typedef enum PrStatus {
    PrStatus_Ok = 0,
    PrStatus_InvalidData,
    PrStatus_MissingSamples, // no sample file next to the song, or it is shorter than the instruments need
    PrStatus_OutOfMemory,
} PrStatus;

typedef struct PrCreateResult {
    PrPlayer* player;
    PrStatus status;
} PrCreateResult;

typedef struct PrTickResult {
    bool looped; // the first voice reached the end of its stream: the eagleplayer calls SongEnd and the song restarts
} PrTickResult;

// What voice (0-based) last started: the period of its note (0 if none) and the instrument number.
typedef struct PrVoiceState {
    int period;
    int instrument;
} PrVoiceState;

typedef struct PrSong {
    uint8_t* data; // the song file; a read past its end is 0
    uint32_t size;
    uint16_t voice_count;
    uint16_t vibrato_count;
    uint16_t envelope_count;
    uint16_t instrument_count;
    int8_t* samples; // nuade_paula_padded_copy of the sample file
    uint32_t sample_size;
} PrSong;

typedef struct PrLoop {
    uint32_t start; // stream position after the LoopStart command
    uint16_t count;
} PrLoop;

typedef struct PrVoice {
    uint32_t stream;          // position in the song file
    uint16_t duration;        // ticks left of the current note
    uint16_t duration_rest;   // remainder of the tempo division, carried into the next note
    uint8_t vibrato_pos;      // position in the vibrato table, steps by vibrato_speed per tick
    uint8_t envelope_pos;     // position in the envelope table, steps by 4 per tick
    uint8_t instrument;       // number of the instrument record
    uint32_t vibrato_table;   // song file offset
    uint32_t envelope_table;  // song file offset
    uint8_t volume;           // 0..63, 0 = silent
    uint8_t vibrato_speed;    // PrCommand_VibratoSpeed
    uint16_t vibrato_divisor; // 10000 / depth
    uint16_t period;          // after portamento, before vibrato
    uint16_t target_period;
    uint16_t portamento_step;
    const int8_t* loop_lc; // the repeat the sample gets one tick after the start
    uint16_t loop_len_words;
    uint16_t loop_period;
    uint8_t fade_volume;
    uint8_t fade_step;
    bool portamento;
    bool vibrato;
    bool fade;
    bool tie;
    bool ended;
    bool legato;
    bool keep_envelope;
    bool keep_vibrato;
    bool instrument_changed; // an instrument command since the last note start (read by the legato notes)
    bool start_pending;      // a note started: switch the DMA on next tick
    bool loop_pending;       // DMA was switched on: write the repeat next tick
    bool dma_running;        // the volume is only written while the channel's DMA runs
    uint32_t loop_depth;
    PrLoop loops[PR_LOOP_DEPTH];
    NuadePaulaChannel paula;
} PrVoice;

struct PrPlayer {
    PrSong song;
    uint16_t tempo;         // PrCommand_Tempo * 16
    uint16_t master_volume; // 0..63, the fade-out of the whole song
    uint16_t master_target;
    int16_t master_step;
    uint16_t master_counter;
    uint16_t master_period; // ticks per step of the master fade
    PrVoice voices[PR_VOICES];
};

// The eagleplayer's Check2 on the song file, plus what this port needs: the voice streams only use what the replayer
// models (notes inside its period table, instruments and tables that exist).
bool pr_probe(const uint8_t* data, size_t size);

// Parses the song (the eagleplayer's InitPlayer) and loads its sample file through `io`.
PrCreateResult pr_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void pr_destroy(PrPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool pr_start(PrPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
PrTickResult pr_tick(PrPlayer* player, NuadeVoices* voices);

// Number of voices (streams) the song uses, 1..PR_VOICES.
uint32_t pr_voice_count(const PrPlayer* player);
PrVoiceState pr_voice_state(const PrPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PrPlayer* pr_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
