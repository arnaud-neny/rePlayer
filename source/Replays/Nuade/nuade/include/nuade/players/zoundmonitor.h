// SPDX-License-Identifier: MIT
// ZoundMonitor player (AJ of Activas, 1988), translated from the UADE eagleplayer (Wanted Team).

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

#define ZM_VOICES 4
#define ZM_ROWS 32
#define ZM_INSTRUMENTS 16

// The CIA clock: the timebase of ZM_TICK_LENGTH.
#define ZM_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / ZM_TIMEBASE_HZ seconds.
#define ZM_TICK_LENGTH 14187

#define ZM_NAME_SIZE 40
// InitPlayer loads samples for the first 15 instruments only; the 16th keeps the pointer its file holds.
#define ZM_LOADED_INSTRUMENTS 15

typedef struct ZmPlayer ZmPlayer;

typedef enum ZmStatus {
    ZmStatus_Ok = 0,
    ZmStatus_InvalidData,
    ZmStatus_MissingSample, // a named instrument has no file in Samples/ (UADE refuses the song too)
    ZmStatus_OutOfMemory,
} ZmStatus;

typedef struct ZmCreateResult {
    ZmPlayer* player; // 0 on failure
    ZmStatus status;
    char missing_sample[48]; // the instrument name, for ZmStatus_MissingSample
} ZmCreateResult;

typedef struct ZmTickResult {
    bool looped; // the song wrapped to its loop position during this tick
} ZmTickResult;

typedef struct ZmPosition {
    int order; // song position
    int row;
    int speed; // ticks per row
} ZmPosition;

typedef struct ZmVoiceState {
    int note;       // period table index of the last note started (after transpose), 0 if none
    int instrument; // 1-based instrument of the voice, 0 if none
} ZmVoiceState;

// One pattern cell as stored (no transposes applied).
typedef struct ZmCell {
    uint8_t note;       // 1..36 period table index, 0x3F rest, 0 none
    uint8_t instrument; // 1-based, 0 none
    uint8_t control;    // ZmControl bits
    int8_t volume;      // added to the instrument volume
    uint8_t param;      // arpeggio notes, slide speed or portamento length
    bool legato;        // no retrigger while the instrument stays the same
} ZmCell;

typedef enum ZmControl {
    ZmControl_None = 0,
    ZmControl_Arpeggio = 1, // with Slide: portamento to the next row's note
    ZmControl_Slide = 2,
    ZmControl_NoNoteTranspose = 4,
    ZmControl_NoInstrumentTranspose = 8,
} ZmControl;

typedef struct ZmInstrument {
    char name[ZM_NAME_SIZE + 1];
    const int8_t* sample; // 0 when the instrument has no loaded sample
    int8_t volume;
    uint16_t length;      // words
    uint16_t loop_length; // words
    uint16_t loop_start;  // words
} ZmInstrument;

typedef struct ZmModule {
    uint8_t* data; // the file as loaded
    uint32_t size;
    uint32_t moved_size; // instruments, positions and patterns: the block InitPlayer moves over the header
    uint32_t patterns_offset;
    int position_count;
    int pattern_count;
    uint8_t start_position;
    uint8_t end_position; // the song wraps when it reaches this position, which never plays
    uint8_t speed;
    ZmInstrument instruments[ZM_INSTRUMENTS];
    int8_t* samples[ZM_LOADED_INSTRUMENTS]; // owned
} ZmModule;

typedef struct ZmVoice {
    int16_t volume; // AUDxVOL as last computed, 0..64
    int16_t slide;  // period change per tick
    uint16_t period;
    uint16_t dma_bit;
    uint16_t length;      // words
    uint16_t loop_length; // words
    const int8_t* sample;
    const int8_t* loop;
    uint8_t note;
    uint8_t control;
    uint8_t arpeggio[3]; // note offsets; the first is always 0
    int16_t instrument_volume;
    bool porta_pending; // the next row of this voice is the portamento target, not a new note
    uint16_t counter;   // arpeggio / portamento tick count
    int16_t porta_delta;
    uint16_t porta_ticks;
    uint8_t legato_instrument;
    uint8_t instrument;

    NuadePaulaChannel paula;
} ZmVoice;

typedef struct ZmSong {
    uint8_t tick;
    uint8_t row;
    uint8_t position;
    uint8_t loop_position;
    uint8_t end_position;
    uint16_t dma_mask; // channels to start at the next tick
    ZmVoice voices[ZM_VOICES];
    bool looped;
    int shown_position;
    int shown_row;
} ZmSong;

struct ZmPlayer {
    ZmModule module;
    ZmSong song;
};

// The eagleplayer's check: the header sizes fit the file and the sample path ("dfN:" or "?amp...") follows the
// patterns.
bool zm_probe(const uint8_t* data, size_t size);

// Loads the module and its samples, which are external: `<directory of path>/Samples/<instrument name>`, any case.
ZmCreateResult zm_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void zm_destroy(ZmPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool zm_start(ZmPlayer* player, NuadeVoices* voices);

ZmTickResult zm_tick(ZmPlayer* player, NuadeVoices* voices);

// 0-based; the instrument's name (its sample file name), "" when unused or out of range.
const char* zm_instrument_name(const ZmPlayer* player, int instrument);

// The row decoded by the last row tick (position 0, row 0 before the first one).
ZmPosition zm_position(const ZmPlayer* player);
int zm_order_count(const ZmPlayer* player);

ZmVoiceState zm_voice_state(const ZmPlayer* player, int voice);

// `order` 0 .. zm_order_count - 1, `voice` 0..3, `row` 0 .. ZM_ROWS - 1.
ZmCell zm_pattern_cell(const ZmPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
ZmPlayer* zm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
