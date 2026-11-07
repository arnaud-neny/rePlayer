// SPDX-License-Identifier: MIT
// Mark Cooksey player, old format (the Atari ST conversions): mco. songs, and mcr. replayers with their mcs. data file.

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

#define MCO_VOICES 4

// The CIA clock: the timebase of MCO_TICK_LENGTH.
#define MCO_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / MCO_TIMEBASE_HZ seconds.
#define MCO_TICK_LENGTH 14187

// The voice record has room for 40 bytes of stack: 4-byte frames (call, mark) and 6-byte frames (loop).
#define MCO_STACK_BYTES 40
#define MCO_STACK_FRAME_SIZE_LONG 4
#define MCO_STACK_FRAMES (MCO_STACK_BYTES / MCO_STACK_FRAME_SIZE_LONG)
// The period table. A transposed note reads before it (negative offsets) and no song reads past it, so a copy of the
// words around the table is all a replayer needs: MCO_PERIOD_WORDS_BEFORE words, then the table's
// MCO_PERIOD_WORDS_AFTER.
#define MCO_PERIOD_WORDS_BEFORE 12
#define MCO_PERIOD_WORDS_AFTER 38
#define MCO_PERIOD_WORDS (MCO_PERIOD_WORDS_BEFORE + MCO_PERIOD_WORDS_AFTER)

typedef struct McoPlayer McoPlayer;

typedef enum McoKind {
    McoKind_None,
    McoKind_Data,   // mco.: song data; the replayer is the eagleplayer's
    McoKind_Driver, // mcr.: the replayer; the data is the mcs. file next to it
} McoKind;

typedef enum McoStatus {
    McoStatus_Ok = 0,
    McoStatus_InvalidData,
    McoStatus_MissingData, // the mcs. file of an mcr. replayer is not there
    McoStatus_OutOfMemory,
} McoStatus;

typedef struct McoCreateResult {
    McoPlayer* player; // 0 on failure
    McoStatus status;
} McoCreateResult;

typedef struct McoTickResult {
    bool looped; // mcr. replayers: every voice has run an end or goto command (the eagleplayer's SongEnd)
    bool ended;  // every voice has stopped (the SongEnd of the replayer in the eagleplayer)
} McoTickResult;

typedef struct McoVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // sample number of the last note started, -1 if none
} McoVoiceState;

typedef struct McoModule {
    uint8_t* data;      // the data (after the ST header) in a nuade_paula_padded_copy of size + SAMPLE_HEADER_BYTES
    size_t size;        // without the padding
    uint32_t sequences; // data offset of the sub-sequence table
    uint32_t samples;   // data offset of the sample table
    int subsong_count;
    uint16_t periods[MCO_PERIOD_WORDS]; // the words around the period table
    uint16_t volume_scale;              // the master volume
    uint16_t start_volume[MCO_VOICES];  // the volume register value a voice starts with
    bool retune_sets_volume;            // retune writes a long, which sets AUDxVOL to the next table entry
    bool reports_song_end;              // end and goto commands call the eagleplayer's SongEnd check
} McoModule;

typedef enum McoFrameKind {
    McoFrameKind_Return, // a call: where to go back to
    McoFrameKind_Mark,   // a mark: the position the stream can jump back to
    McoFrameKind_Loop,   // a loop: the repeats left and where it starts
} McoFrameKind;

typedef struct McoFrame {
    McoFrameKind kind;
    uint32_t position; // data offset
    uint16_t repeats;
} McoFrame;

typedef struct McoVoice {
    int16_t count;   // ticks until the next command, negative: stopped
    uint32_t stream; // data offset of the next command
    uint32_t sample; // data offset of the sample (a word count, then the data), 0 until an instrument command
    int transpose;   // bytes added to the period table pointer
    uint16_t volume; // the volume register value of the last volume command
    McoFrame stack[MCO_STACK_FRAMES];
    int depth;
    int stack_bytes;
    bool reached_end; // the voice ran an end or goto command since the song last ended
    int shown_note;
    int shown_instrument;
    NuadePaulaChannel paula;
} McoVoice;

struct McoPlayer {
    McoModule module;
    McoVoice voices[MCO_VOICES];
    bool song_ended; // set when the last voice reaches its end or goto command
};

// The eagleplayers' Check2 (Old: the data file of an mco. song; the other: an mcr. replayer) and the file name rule
// of eagleplayer.conf (an mco. or mcr. prefix).
McoKind mco_probe(const uint8_t* data, size_t size, const char* filename);

// `path` is the url of the song; an mcr. replayer reads `mcs.<name>` from the same directory (any case).
McoCreateResult mco_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void mco_destroy(McoPlayer* player);

int mco_subsong_count(const McoPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mco_start(McoPlayer* player, int subsong, NuadeVoices* voices);

McoTickResult mco_tick(McoPlayer* player, NuadeVoices* voices);

McoVoiceState mco_voice_state(const McoPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
McoPlayer* mco_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
