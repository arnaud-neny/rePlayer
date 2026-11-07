// SPDX-License-Identifier: MIT
// Synth Pack (Karsten Obarski, 1989-92), translated from the UADE eagleplayer "SynthPack" (Wanted Team).
// The module formats and the replayers are described in players/synthpack/.

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

#define SP_VOICES 4

// The CIA clock: the timebase of SP_TICK_LENGTH.
#define SP_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SP_TIMEBASE_HZ seconds.
#define SP_TICK_LENGTH 14187

typedef struct SpPlayer SpPlayer;

typedef enum SpStatus {
    SpStatus_Ok = 0,
    SpStatus_InvalidData,    // not a module this port reproduces; UADE plays it
    SpStatus_MissingSamples, // the sample set could not be read
    SpStatus_OutOfMemory,
} SpStatus;

typedef struct SpCreateResult {
    SpPlayer* player; // 0 on failure
    SpStatus status;
} SpCreateResult;

typedef struct SpTickResult {
    bool looped; // the replayer reached a point where the eagleplayer calls SongEnd
    bool ended;  // the song was stopped and every voice has died away
} SpTickResult;

// What voice (0-based Paula channel) last started: the note (semitones from C-0, -1 if none yet) and the instrument.
typedef struct SpVoiceState {
    int note;
    int instrument;
} SpVoiceState;

typedef struct SpMemory {
    uint8_t* data;
    uint32_t size;
} SpMemory;

typedef struct SpSongEvents {
    bool song_end; // SongEnd was called: the song wrapped
    bool stopped;  // the sequencer stopped: the voices are released
} SpSongEvents;

typedef struct SpChannel {
    NuadePaulaChannel paula;
    bool latched;   // a note's sample was written this tick: its DMA goes off at the end of the tick
    bool start_dma; // its DMA goes on at the start of the next tick
} SpChannel;

typedef struct SpHardware {
    SpChannel channel[SP_VOICES];
} SpHardware;

typedef enum SpAdsrPhase {
    SpAdsrPhase_Attack = 0,
    SpAdsrPhase_Decay,
    SpAdsrPhase_Sustain, // holds until the key is released
    SpAdsrPhase_Release,
    SpAdsrPhase_Idle,
} SpAdsrPhase;

typedef struct SpAdsr {
    SpAdsrPhase phase;
    int16_t counter; // counts down by 2 a tick; the step is taken when it goes negative
    int16_t level;
} SpAdsr;

typedef enum SpRestartStep {
    SpRestart_None = 0,
    SpRestart_Latch,  // write the first run
    SpRestart_Repeat, // write the loop, after the DMA started
} SpRestartStep;

typedef struct SpVoice {
    bool released;
    uint8_t instrument;
    uint8_t arp_param; // of the last note: the arpeggio's nibbles
    uint16_t arp_note; // of the last note: the arpeggio adds to it
    uint16_t base_period;
    uint16_t period;
    uint16_t volume; // of the last note
    int16_t slide;   // pitch slide, in periods
    SpRestartStep restart;
    SpAdsr adsr;
    int16_t level; // the envelope level after the volume LFO
    int note;      // for the display: -1 before the first note
} SpVoice;

// A position plays one pattern line; `control` says what follows it.
typedef struct SpPosition {
    uint8_t rows;    // rows of the pattern line (0 plays one)
    uint8_t pattern; // pattern line
    uint8_t control; // 0..0x7F: plays the pattern line control + 1 times, else an SpControl
    uint8_t voices;  // voices that play, one bit each
} SpPosition;

// The song's sequencer state, the same in both layouts.
typedef struct SpSequencer {
    bool running;
    unsigned position;
    unsigned row;
    unsigned loop_point;
    unsigned repeat;
    unsigned tick;  // within the row
    unsigned speed; // ticks per row
    SpPosition entry;
} SpSequencer;

typedef enum SpLayout {
    SpLayout_Words,
    SpLayout_Longs,
} SpLayout;

typedef struct SpLongLfo {
    uint16_t delay; // counts down by SP_LFO_DELAY_UNIT a tick while its high byte is not 0
    uint16_t speed; // added to the phase each tick
    uint16_t phase; // the high byte indexes the table
    uint8_t wave;
    bool table; // set when a note starts; before that the LFO reads 0
} SpLongLfo;

typedef struct SpLongVoice {
    SpVoice base;
    uint8_t effect; // of the last event: the arpeggio pattern is its low 3 bits
    uint8_t arp_step;
    SpLongLfo volume_lfo;
    SpLongLfo pitch_lfo;
    SpLongLfo morph;
    uint32_t events; // offset in the song of the event the column waits for
} SpLongVoice;

typedef struct SpLongState {
    const SpMemory* song;
    const SpMemory* set;
    SpSequencer sequencer;
    SpLongVoice voice[SP_VOICES]; // by sequencer column
    SpSongEvents events;
} SpLongState;

typedef struct SpWordLfo {
    uint16_t delay;   // counts down by SP_LFO_DELAY_UNIT a tick while its high byte is not 0
    uint16_t counter; // counts down by SP_LFO_DELAY_UNIT a tick; the phase moves on when it goes negative
    uint16_t phase;   // byte offset into the table, 8 at a time
    uint8_t wave;
    bool table; // set when a note starts; before that the LFO reads 0
} SpWordLfo;

typedef struct SpWordVoice {
    SpVoice base;
    SpWordLfo volume_lfo;
    SpWordLfo pitch_lfo;
    int16_t sweep_counter;
    uint16_t sweep_position; // of the waveform, in bytes
    bool sweep_down;
} SpWordVoice;

typedef struct SpWordState {
    const SpMemory* song;
    const SpMemory* set;
    SpSequencer sequencer;
    SpWordVoice voice[SP_VOICES]; // by Paula channel
    SpSongEvents events;
} SpWordState;

struct SpPlayer {
    SpLayout layout;
    SpMemory song;
    SpMemory set;
    int subsong_count;
    SpHardware hardware;
    SpWordState words;
    SpLongState longs;
    bool stopped; // the sequencer stopped: the song ends when the voices have died away
};

// The eagleplayer's check: "OBISYNTHPACK", and at 0x50c either the word 0x0100 or a zero long.
bool sp_probe(const uint8_t* data, size_t size);

// Loads the module and its sample set ("smp.<name>" for a song "osp.<name>", else "smp.set" in the song's directory).
SpCreateResult sp_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void sp_destroy(SpPlayer* player);

int sp_subsong_count(const SpPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool sp_start(SpPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
SpTickResult sp_tick(SpPlayer* player, NuadeVoices* voices);

// Ticks one row of the song spans at the start; the speed of a song can change in its patterns.
uint32_t sp_grid_ticks(const SpPlayer* player);
SpVoiceState sp_voice_state(const SpPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SpPlayer* sp_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
