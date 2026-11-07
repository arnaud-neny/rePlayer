// SPDX-License-Identifier: MIT
// JamCracker module player

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JC_VOICES 4

// The CIA clock: the timebase of JC_TICK_LENGTH.
#define JC_TIMEBASE_HZ 709379
// One player tick (CIA timer value $376B, 50 Hz), in 1 / JC_TIMEBASE_HZ seconds.
#define JC_TICK_LENGTH 14187

#define JC_NAME_LENGTH 31

typedef struct JcPlayer JcPlayer;

typedef struct JcTickResult {
    bool looped; // the song order wrapped during this tick
} JcTickResult;

typedef struct JcPosition {
    int order; // song position
    int row;
    int rows;  // rows of the pattern at that position
    int speed; // ticks per row
} JcPosition;

typedef struct JcVoiceState {
    int note;       // note byte (period table index + 1) of the last note started on the voice, 0 if none
    int instrument; // its instrument byte as the player reads it (signed)
} JcVoiceState;

// One note of a pattern row, the bytes as stored.
typedef struct JcCell {
    uint8_t period; // note byte, 0 = none
    uint8_t instrument;
    uint8_t speed;
    uint8_t arpeggio;
    uint8_t vibrato;
    uint8_t phase;
    uint8_t volume;
    uint8_t porta;
} JcCell;

typedef struct JcInstrument {
    char name[JC_NAME_LENGTH + 1];
    uint8_t flags;
    uint32_t size;   // bytes of wave data present
    int8_t* address; // owned, writable: an AM instrument rewrites its output windows
} JcInstrument;

typedef struct JcPattern {
    uint16_t size;            // rows in the file layout
    uint16_t rows;            // rows played
    const uint8_t* rows_data; // size * 4 voices * NOTE_SIZE
} JcPattern;

typedef struct JcModule {
    JcInstrument* instruments;
    uint16_t instrument_count;
    JcPattern* patterns;
    uint16_t pattern_count;
    uint16_t* songtable;
    uint16_t songlen;
} JcModule;

typedef struct JcVoice {
    uint16_t waveoffset;
    int8_t* insaddress;
    uint16_t inslen;      // words
    int peraddress;       // period table index of the note
    int shown_note;       // note byte of the last note started, 0 if none
    int shown_instrument; // its instrument byte
    int16_t pers[3];
    int16_t por;
    int16_t deltapor;
    int16_t porlevel;
    int16_t vib;
    int16_t deltavib;
    int16_t vol;
    int16_t deltavol;
    int16_t vollevel;
    int16_t phase;
    int16_t deltaphase;
    uint8_t vibcnt;
    uint8_t vibmax;
    uint8_t flags;
    bool triggered;  // the new row restarts this voice's DMA
    uint16_t period; // Paula period register
    uint16_t volume; // Paula volume register
} JcVoice;

struct JcPlayer {
    JcModule module;
    uint8_t wait;
    uint8_t waitcnt;
    uint8_t notecnt;
    const uint8_t* address; // next row to play
    uint16_t songpos;
    // The row decoded by the last row tick: the pattern's song position, the row in it and the pattern's rows.
    uint16_t shown_songpos;
    uint16_t shown_row;
    uint16_t shown_rows;
    JcVoice variables[JC_VOICES];
};

// True when the data starts with the "BeEp" id and the module header fits.
bool jc_probe(const uint8_t* data, size_t size);

JcPlayer* jc_create(const uint8_t* data, size_t size);
void jc_destroy(JcPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool jc_start(JcPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
JcTickResult jc_tick(JcPlayer* player, NuadeVoices* voices);

int jc_instrument_count(const JcPlayer* player);
// 0-based; the instrument's name, or "" when out of range.
const char* jc_instrument_name(const JcPlayer* player, int instrument);

// The row decoded by the last row tick (position 0, row 0 before the first one).
JcPosition jc_position(const JcPlayer* player);
int jc_order_count(const JcPlayer* player);
// Rows played in the pattern at song position `order`.
int jc_pattern_rows(const JcPlayer* player, int order);

JcVoiceState jc_voice_state(const JcPlayer* player, int voice);

// `order` 0 .. jc_order_count - 1, `voice` 0..3, `row` 0 .. jc_pattern_rows - 1.
JcCell jc_pattern_cell(const JcPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
JcPlayer* jc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
