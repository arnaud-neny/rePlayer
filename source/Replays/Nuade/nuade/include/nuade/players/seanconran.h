// SPDX-License-Identifier: MIT
// Sean Conran player for the Amiga modules that carry their replayer ("*.scr", prefix scr).

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

#define SCR_VOICES 4

// The CIA clock: the timebase of SCR_TICK_LENGTH.
#define SCR_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / SCR_TIMEBASE_HZ seconds.
#define SCR_TICK_LENGTH 14187

// The command bytes of a voice are 0, 4, ... 0x38; the replayer jumps through a table with one slot for each.
#define SCR_COMMAND_SLOTS 15

typedef struct ScrPlayer ScrPlayer;

typedef struct ScrTickResult {
    bool looped; // the first voice's track list has reached its marker (the eagleplayer's SongEnd)
    bool ended;  // the replayer has stopped
} ScrTickResult;

// What the audio interrupt handler of a voice writes to AUDxLC / AUDxLEN until a sample command patches it.
typedef struct ScrHandlerLoop {
    uint32_t start; // SCR_OUTSIDE_MODULE when the handler holds an address of the original program's memory
    uint16_t words;
} ScrHandlerLoop;

// How a build's voices differ. Everything else of the replayer is the same in the builds the port knows, and the
// code hash proves it.
typedef struct ScrBuild {
    uint64_t code_hash;
    uint32_t code_start;               // where the code starts in the module: the hash does not depend on it
    uint8_t start_master_volume;       // the master volume after the init
    uint8_t start_pitch_bias;          // the pitch bias of the voices after the init
    uint32_t volume_table;             // 16 words
    uint32_t song_table;               // 8 bytes for each subsong: the voices' track lists as offsets; 0: one subsong
    uint32_t track_lists[SCR_VOICES];  // the track lists of the one subsong of a build without a song table
    uint32_t track_tables[SCR_VOICES]; // for each voice the stream offsets of its track list entries
    bool track_entries_doubled;        // a track list byte is the entry number times 2
    bool length_ends_at_zero;          // a length reads commands when it reaches 0 and ends a note at 1 (else -1 and 0)
    bool volume_nibble;                // the volume sequence's value is masked to 4 bits
    bool pitch_index_bytes;            // the pitch index is a byte offset into the period table (else a word index)
    bool marker_rechecked;             // the entry a marker jumps back to may be a marker again
    bool pitch_shifts_ascend;          // the voices start with the pitch shifts 5, 6, 7, 8 (else 8, 7, 6, 5)
    bool volume_start_zero;            // the voices start with the volume indexes 0..3 (else 1..4)
    bool release_clears_volume;        // the end of a note sets its volume index to 0
    bool audio_interrupts;             // the build chains sample repeats with the audio interrupts
    ScrHandlerLoop handler_loops[SCR_VOICES];
    char commands[SCR_VOICES][SCR_COMMAND_SLOTS + 1];
} ScrBuild;

typedef struct ScrModule {
    uint8_t* data; // the module with Paula's padding behind it; the replayer does not write to it
    size_t size;
    const ScrBuild* build;
    uint32_t subsong_count;
    uint32_t written_from; // the bytes [written_from, written_to) are the replayer's code and variables: it writes to
    uint32_t written_to;   // them as it runs, so the module's data must not lie there
} ScrModule;

typedef struct ScrSequence {
    uint32_t at;
    uint32_t restart;
    bool restart_pending; // the next step starts the sequence over
} ScrSequence;

typedef struct ScrPitchShift {
    uint8_t count; // 1..8
    bool left;
} ScrPitchShift;

typedef struct ScrVoice {
    uint32_t commands_at; // next byte of the command stream
    uint32_t track_at;    // next byte of the track list
    uint8_t length;       // ticks left of the note, counts down; at the end the voice reads commands
    bool legato;          // the command after the note is a note: the DMA stays on
    bool call_armed;
    uint32_t call_return;
    bool envelope_on; // the volume and pitch sequences run
    ScrSequence volumes;
    ScrSequence pitches;
    int8_t volume_base;
    uint8_t volume_index; // the volume sequence's last value, index of the volume table
    uint8_t pitch_bias;
    ScrPitchShift pitch_shift;
    bool sample_set; // the voice's AUDxLC has been written
    bool loop_armed; // the audio interrupt of the channel is enabled
    uint32_t loop_start;
    uint16_t loop_words;
    uint32_t wraps_seen;
    bool halted; // the voice reads commands for ever in the replayer, or the data is not modelled
    NuadePaulaChannel paula;
} ScrVoice;

struct ScrPlayer {
    ScrModule* module;
    ScrVoice voices[SCR_VOICES];
    uint8_t master_volume;
    bool playing;
    bool stop_pending; // the stop routine has not run yet
    bool hung;         // the replayer loops for ever inside a tick: nothing more is written
    bool looped;
    bool proving; // a dry run: reads of data the port does not model set `unmodelled`
    bool unmodelled;
};

// True when the eagleplayer plays the module and this port reproduces it: the eagleplayer's checks, a replayer build
// that the port knows, and sequence data the port models.
bool scr_probe(const uint8_t* data, size_t size);

// null when scr_probe is false.
ScrPlayer* scr_create(const uint8_t* data, size_t size);
void scr_destroy(ScrPlayer* player);

int scr_subsong_count(const ScrPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool scr_start(ScrPlayer* player, int subsong, NuadeVoices* voices);

ScrTickResult scr_tick(ScrPlayer* player, NuadeVoices* voices);

// The player's state behind a song it opened; 0 when the song is another player's.
ScrPlayer* scr_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
