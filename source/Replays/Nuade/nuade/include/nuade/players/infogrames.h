// SPDX-License-Identifier: MIT
// Infogrames music (Rob Hubbard 2 driver, adapted by Andy Silva), translated from the UADE eagleplayer "Infogrames".
// A song is a ".dum" file (sequences) with an ".ins" file next to it (samples).

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

#define IG_VOICES 4

// The CIA clock: the timebase of the tick length.
#define IG_TIMEBASE_HZ 709379

typedef struct IgPlayer IgPlayer;

typedef enum IgStatus {
    IgStatus_Ok = 0,
    IgStatus_InvalidData,
    IgStatus_OutOfMemory,
    IgStatus_MissingSamples,
} IgStatus;

typedef struct IgCreateResult {
    IgPlayer* player;
    IgStatus status;
} IgCreateResult;

typedef struct IgTickResult {
    bool ended; // every voice has reached the end of its track list
} IgTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the sample number.
typedef struct IgVoiceState {
    int note;
    int instrument;
} IgVoiceState;

typedef struct IgFile {
    uint8_t* data;
    uint32_t size;
} IgFile;

typedef struct IgSongHeader {
    int32_t base;                   // the header's own offset: the offsets below are relative to it
    uint8_t tempo;                  // ticks per step
    int32_t amplitude_envelopes;    // IG_ENVELOPE_RECORD_SIZE bytes each
    int32_t pitch_envelopes;        // the same
    int32_t track_lists[IG_VOICES]; // one list of pattern numbers per voice
    int32_t pattern_offsets;        // signed words, relative to `base`
} IgSongHeader;

// An envelope in progress. It steps through the record's segments: each one adds its delta to `level` once per repeat
// and lasts `ticks` ticks.
typedef struct IgEnvelope {
    int32_t segments; // IG_NO_OFFSET until one is chosen
    bool loops;
    int8_t loop_step;
    int16_t offset;  // subtracted from the value; each loop adds loop_step
    int16_t level;   // the sum of the deltas
    uint8_t segment; // index of the current segment
    uint8_t tick;
    uint8_t repeat;
    bool add_delta; // the segment's delta is due
    bool done;      // the last segment is over and the envelope does not loop
} IgEnvelope;

typedef struct IgVoice {
    int32_t track_start;
    int32_t track;   // next entry of the track list
    int32_t pattern; // next byte of the pattern
    bool restart;    // start the track list over on the next step
    bool stopped;
    uint8_t step_counter; // steps down to the next pattern byte
    uint8_t step_reload;
    uint8_t transpose;
    uint16_t period;
    IgEnvelope amplitude;
    IgEnvelope pitch;
    int display_note;
    int display_instrument;
} IgVoice;

typedef struct IgSong {
    IgSongHeader header;
    uint8_t tempo_counter;
    uint8_t tempo_reload;
    int sample_request; // sample number a voice's step asked for, IG_NO_SAMPLE if none
    IgVoice voices[IG_VOICES];
    NuadePaulaChannel paula[IG_VOICES];
} IgSong;

struct IgPlayer {
    IgFile module;
    IgFile samples;
    uint32_t tick_length;
    IgSong song;
};

// The eagleplayer's check: the first word is the offset of the song header, the header's second word leads to the first
// amplitude envelope, which starts with 0, 15.
bool ig_probe(const uint8_t* data, size_t size);

// Loads the song and its ".ins" sample file (the eagleplayer's ExtLoad) through `io`; `path` is the song's path.
IgCreateResult ig_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void ig_destroy(IgPlayer* player);

// Length of one tick in 1 / IG_TIMEBASE_HZ seconds: the eagleplayer picks the timer by the song's checksum.
uint32_t ig_tick_length(const IgPlayer* player);

// (Re)start the song; the player drives `voices` (Paula channel n = voice n).
bool ig_start(IgPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
IgTickResult ig_tick(IgPlayer* player, NuadeVoices* voices);
IgVoiceState ig_voice_state(const IgPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
IgPlayer* ig_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
