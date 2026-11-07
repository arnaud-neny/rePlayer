// SPDX-License-Identifier: MIT
// AMOS Music Bank player (Dec 2004 eagleplayer), translated from the UADE eagleplayer.
//
// The eagleplayer converts the AMOS music bank to a four-channel ProTracker song in memory and plays that with a
// ProTracker replayer (per-tick effects, the PT 2.x extended commands, funk repeat).

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

#define AMOS_VOICES 4
#define AMOS_ROWS 64
#define AMOS_SAMPLES 31

// The CIA clock: the timebase of the tick length.
#define AMOS_TIMEBASE_HZ 709379

#define AMOS_TITLE_SIZE 16
#define AMOS_ORDER_MAX 128
#define AMOS_NAME_SIZE 16

typedef struct AmosPlayer AmosPlayer;

typedef struct AmosTickResult {
    bool looped;     // the replayer called SongEnd: it came back to a row it had played
    uint32_t length; // until the next tick, in AMOS_TIMEBASE_HZ units
} AmosTickResult;

typedef struct AmosPosition {
    int order; // position in the order list
    int row;
    int speed; // ticks per row
} AmosPosition;

typedef struct AmosVoiceState {
    int note;   // index in the period table of the last note started (0 = C-1), -1 if none
    int sample; // 1-based sample of the voice, 0 if none
} AmosVoiceState;

// One converted pattern cell, in ProTracker terms.
typedef struct AmosCell {
    uint16_t period; // 12 bits, 0 none
    uint8_t sample;  // 0 none, 1..31
    uint8_t effect;  // PT effect number 0..15
    uint8_t param;
} AmosCell;

// The bank as loaded; reads outside it give 0.
typedef struct AmosBank {
    const uint8_t* data;
    size_t size;
} AmosBank;

// A converted PT cell. The sample number is 0..31.
typedef struct AmosPtCell {
    uint16_t period; // 12 bits
    uint8_t sample;
    uint8_t effect;
    uint8_t param;
} AmosPtCell;

typedef struct AmosPattern {
    AmosPtCell cell[AMOS_ROWS][AMOS_VOICES];
} AmosPattern;

// A sample slot of the PT song header.
typedef struct AmosSample {
    char name[AMOS_NAME_SIZE + 1];
    uint16_t length; // words
    uint8_t finetune;
    uint8_t volume;
    uint16_t loop_start;  // words
    uint16_t loop_length; // words
    uint32_t offset;      // in the sample memory
} AmosSample;

typedef struct AmosModule {
    uint8_t* bank_data; // the file as loaded (owned)
    AmosBank bank;
    size_t samples_offset; // the first sample's data in the bank
    char title[AMOS_TITLE_SIZE + 1];
    uint8_t order[AMOS_ORDER_MAX];
    uint8_t order_length;
    AmosPattern* patterns;
    uint32_t pattern_count;
    int sample_count; // instruments of the bank
    AmosSample samples[AMOS_SAMPLES];
} AmosModule;

// The replayer's voice record (the PT channel structure).
typedef struct AmosVoice {
    AmosPtCell cell; // the row's note and command
    bool cell_set;   // the cell is not all zero
    int8_t* start;
    uint16_t length; // words
    int8_t* loop_start;
    uint16_t loop_length; // words
    uint16_t period;
    uint8_t finetune;
    uint8_t volume;
    bool dma_pending; // the note started in this row waits for its DMA start
    bool porta_down;  // the period has to fall to reach the target
    uint8_t porta_speed;
    uint16_t porta_target;
    uint8_t vibrato_depth;
    uint8_t vibrato_speed;
    uint8_t vibrato_pos;
    uint8_t vibrato_waveform;
    bool vibrato_continues; // a new note does not restart the vibrato
    bool glissando;         // a tone portamento snaps to the notes
    uint8_t funk_speed;     // invert loop, 0 = off
    uint8_t loop_row;
    uint8_t loop_count;
    uint8_t funk_offset;
    int8_t* funk_position; // the last byte of the loop the invert loop flipped

    int note;   // display: period table index of the last note
    int sample; // display: 1-based sample

    NuadePaulaChannel paula;
} AmosVoice;

typedef struct AmosSong {
    uint32_t tick;
    uint32_t speed;
    uint16_t timer; // CIA timer value of the tick
    uint32_t song_position;
    uint32_t row;
    uint8_t break_row;
    bool break_pending;
    bool loop_pending;
    uint8_t slide_mask;
    uint8_t delay_request;
    uint8_t delay_count;
    bool visited[AMOS_ORDER_MAX][AMOS_ROWS]; // rows played since the song start or the last SongEnd
    AmosSample samples[AMOS_SAMPLES];        // the sample headers as InitSound leaves them
    AmosVoice voices[AMOS_VOICES];
    bool looped;
    int shown_order;
    int shown_row;
} AmosSong;

struct AmosPlayer {
    AmosModule module;
    int8_t* sample_memory;     // the samples, in the order of the slots, followed by zeros
    size_t sample_memory_size; // all of it, the Paula tail too: the invert loop writes there
    AmosSong song;
};

// The eagleplayer's check: the bank header ("AmBk" + the music bank mark, or the same without the magic) and the
// first position list's terminator.
bool amos_probe(const uint8_t* data, size_t size);

// Converts the bank like InitPlayer; null when it is not a music bank or runs short.
AmosPlayer* amos_create(const uint8_t* data, size_t size);
void amos_destroy(AmosPlayer* player);

// InitSound: (re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool amos_start(AmosPlayer* player, NuadeVoices* voices);

AmosTickResult amos_tick(AmosPlayer* player, NuadeVoices* voices);

// The song title (16 bytes) and the sample names ("" for an unused slot, 0-based).
const char* amos_title(const AmosPlayer* player);
const char* amos_sample_name(const AmosPlayer* player, int sample);

// The row played by the last row tick (position 0, row 0 before the first one).
AmosPosition amos_position(const AmosPlayer* player);
int amos_order_count(const AmosPlayer* player);

AmosVoiceState amos_voice_state(const AmosPlayer* player, int voice);

// `order` 0 .. amos_order_count - 1, `voice` 0..3, `row` 0 .. AMOS_ROWS - 1.
AmosCell amos_pattern_cell(const AmosPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
AmosPlayer* amos_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
