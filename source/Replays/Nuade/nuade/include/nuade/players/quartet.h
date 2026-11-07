// SPDX-License-Identifier: MIT
// Quartet player, translated from the UADE eagleplayer "Quartet" (Wanted Team, after Dan Lennard's Amiga replayer). A
// song is a "qpa" file (a tempo byte and rows of four voices) whose samples are in an "SMP.<name>" file (or "SMP.set")
// in the same directory; the replayer is in the eagleplayer.

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

#define QT_VOICES 4

// The CIA clock: the timebase of QT_TICK_LENGTH.
#define QT_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / QT_TIMEBASE_HZ seconds.
#define QT_TICK_LENGTH 14187

#define QT_SAMPLES 16

typedef struct QtPlayer QtPlayer;

typedef enum QtStatus {
    QtStatus_Ok = 0,
    QtStatus_InvalidData,
    QtStatus_MissingSamples, // no sample set next to the song, or it is not a sample set the replayer accepts
    QtStatus_OutOfMemory,
} QtStatus;

typedef struct QtCreateResult {
    QtPlayer* player;
    QtStatus status;
} QtCreateResult;

typedef struct QtTickResult {
    bool looped; // the replayer called SongEnd (the row with a 0xFFFF word was reached)
    bool ended;  // the song reached something the replayer does with memory this port does not model
} QtTickResult;

// What voice (0-based) last started: the period of its note (0 if none) and the sample number.
typedef struct QtVoiceState {
    int period;
    int instrument;
} QtVoiceState;

typedef struct QtModule {
    const uint8_t* data;
    uint32_t size;
    uint8_t tempo;
    bool loops; // the song is long enough that the replayer restarts it at its end
} QtModule;

typedef struct QtSampleEntry {
    uint32_t start;        // offset in the set
    uint16_t length_words; // 0x32 for a sample of at most 0x32 words
    uint16_t offset;       // added to the start of a note
    uint16_t length_override;
} QtSampleEntry;

typedef struct QtSamples {
    uint8_t* data; // nuade_paula_padded_copy of the sample set; null: not loaded, nothing is checked against it
    uint32_t size;
    uint16_t loop_mask; // bit n: sample n repeats
    QtSampleEntry entry[QT_SAMPLES];
} QtSamples;

typedef struct QtSlide {
    bool active;
    int16_t quotient;  // added to the period every tick
    int16_t remainder; // |remainder| accumulates; at `length` the period moves one more step in the direction of `sign`
    int16_t sign;
    int16_t accumulated;
    int16_t length; // ticks
} QtSlide;

typedef struct QtVoice {
    uint16_t period;
    uint8_t sample;
    bool has_note;
    QtSlide slide;
    NuadePaulaChannel paula;
} QtVoice;

struct QtPlayer {
    uint8_t* module_copy;
    QtModule module;
    QtSamples samples;
    uint32_t row;
    uint8_t row_counter; // ticks until the next row
    bool unmodeled;      // something the replayer does with memory outside this model happened
    bool looped;
    QtVoice voices[QT_VOICES];
};

// The eagleplayer's Check2 on the song file.
bool qt_probe(const uint8_t* data, size_t size);

// Parses the song (the eagleplayer's InitPlayer), loads its sample set through `io` and refuses a song that makes the
// replayer read memory outside the song or the sample set.
QtCreateResult qt_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void qt_destroy(QtPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool qt_start(QtPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
QtTickResult qt_tick(QtPlayer* player, NuadeVoices* voices);

// Ticks per row (the song's tempo byte).
uint32_t qt_tempo(const QtPlayer* player);
QtVoiceState qt_voice_state(const QtPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
QtPlayer* qt_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
