// SPDX-License-Identifier: MIT
// Protracker4 player (IFF "FORM MODL" ProTracker modules, `ptm`), translated from the UADE eagleplayer.

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

#define PT4_VOICES 4
#define PT4_ROWS 64

// The CIA clock: the timebase of the tick length.
#define PT4_TIMEBASE_HZ 709379

#define PT4_SAMPLES 31
#define PT4_ORDERS 128
#define PT4_INFO_TITLE_SIZE 32

typedef struct Pt4Player Pt4Player;

typedef struct Pt4TickResult {
    bool looped;     // the replayer called the host's SongEnd during this tick
    uint32_t length; // CIA ticks (PT4_TIMEBASE_HZ) until the next tick
} Pt4TickResult;

typedef struct Pt4Position {
    int order;
    int row;
    int speed; // ticks per row
} Pt4Position;

typedef struct Pt4VoiceState {
    int note;   // semitones from C-0 of the last note started, -1 if none
    int sample; // 1-based sample number last selected on the voice, 0 if none
} Pt4VoiceState;

// One pattern cell as stored.
typedef struct Pt4Cell {
    uint16_t period; // 0 none
    uint8_t sample;  // 1-based, 0 none
    uint8_t effect;
    uint8_t param;
} Pt4Cell;

typedef struct Pt4Sample {
    uint32_t start;  // offset in the file
    uint16_t length; // words
    uint8_t finetune;
    uint8_t volume;       // not limited to 64
    uint16_t loop_start;  // words, 0 = no loop
    uint16_t loop_length; // words
} Pt4Sample;

typedef struct Pt4Module {
    uint8_t* mem;      // the file in chip-like memory (padded); the replayer patches sample data in it
    uint8_t* original; // the file as loaded, to restore `mem` at a start
    uint32_t size;     // file size
    uint32_t song;     // offset of the PTDT payload
    uint8_t order_count;
    uint8_t file_song_length; // byte 0x3B6 of the file, which the position jump takes for the song length
    uint8_t orders[PT4_ORDERS];
    uint32_t patterns; // offset of pattern 0
    Pt4Sample samples[PT4_SAMPLES];
    uint16_t bpm;        // start tempo, 0 = 125
    bool tempo_commands; // INFO flags bit 0: Fxx from 0x20 on sets the tempo (otherwise a speed)
    char title[PT4_INFO_TITLE_SIZE + 1];
} Pt4Module;

typedef struct Pt4Voice {
    Pt4Cell cell;           // $00 the cell of this row
    const int8_t* sample;   // $04 the sample start (moved by the sample offset effect), null before the first sample
    uint32_t length;        // $08 words played at a DMA start
    const int8_t* loop;     // $0C
    uint32_t loop_length;   // $10 words
    uint16_t period;        // $14 the note's period (finetuned), the base of the pitch effects
    uint8_t finetune;       // $16
    uint8_t volume;         // $17 not limited to 64
    bool porta_down;        // $1A the tone portamento slides the period down
    uint8_t porta_speed;    // $1B
    uint16_t porta_target;  // $1C
    uint8_t vibrato_speed;  // $1E high nibble
    uint8_t vibrato_depth;  // $1E low nibble
    uint8_t vibrato_pos;    // $1F
    uint8_t tremolo_speed;  // $20 high nibble
    uint8_t tremolo_depth;  // $20 low nibble
    uint8_t tremolo_pos;    // $21
    uint8_t vibrato_wave;   // $22 bits 0..1: 0 sine, 1 ramp down, otherwise square
    bool vibrato_continues; // $22 bit 2: a new note keeps the vibrato position
    uint8_t tremolo_wave;   // $22 bits 4..5
    bool tremolo_continues; // $22 bit 6
    uint8_t glissando;      // $23 low nibble: E3x
    uint8_t funk_speed;     // $23 high nibble: EFx
    uint8_t offset_memory;  // $24 the last 9xx parameter
    uint16_t loop_row;      // $26 E60: the row the pattern loop returns to
    uint8_t loop_count;     // $28
    uint8_t funk_acc;       // $29
    const int8_t* funk_pos; // $2A the loop byte the funk repeat inverted last

    bool started; // got a note on this row: its DMA starts after the row
    int note;     // for the display: semitones from C-0 of the last note started
    uint8_t sample_number;

    NuadePaulaChannel paula;
} Pt4Voice;

typedef struct Pt4Song {
    Pt4Module* module; // the replayer writes into its sample data
    uint32_t pattern;  // offset of the pattern being played
    uint32_t order;
    uint16_t row;        // $2528 the row to play next
    uint32_t counter;    // $251E ticks into the row
    uint16_t speed;      // $2524
    uint16_t timer;      // the CIA timer value of the current tempo
    uint8_t fine_mask;   // $254A E1x / E2x take the low nibble of the parameter. It starts at 0, so the first 1xx / 2xx
                         // of a song slides by nothing
    bool jump_pending;   // $2548 Bxx / Dxx: the next row is jump_row of position `order` + 1
    bool loop_pending;   // $2549 E6x: the next row is jump_row of this pattern
    uint16_t jump_row;   // $2546
    uint8_t delay_set;   // $254B EEx
    uint8_t delay_count; // $254C
    Pt4Voice voices[PT4_VOICES];
    bool looped;
    int shown_order;
    int shown_row;
} Pt4Song;

struct Pt4Player {
    Pt4Module module;
    Pt4Song song;
};

// The eagleplayer's checks (FORM / MODL / VERS, the INFO and PTDT chunks) plus the module features the port models: a
// module the replayer would read outside its data for (8 channels, a sample number past 31, a finetune past 15, a
// sample outside the file) is not accepted, so that UADE plays it.
bool pt4_probe(const uint8_t* data, size_t size);

// 0 on failure.
Pt4Player* pt4_create(const uint8_t* data, size_t size);
void pt4_destroy(Pt4Player* player);

// (Re)start from the first position; the player drives `voices` (Paula channel n = voice n).
bool pt4_start(Pt4Player* player, NuadeVoices* voices);

Pt4TickResult pt4_tick(Pt4Player* player, NuadeVoices* voices);

// The row decoded by the last row tick.
Pt4Position pt4_position(const Pt4Player* player);
// Song positions (the length byte of the module).
int pt4_order_count(const Pt4Player* player);

// The module name (INFO chunk), "" if none.
const char* pt4_title(const Pt4Player* player);

Pt4VoiceState pt4_voice_state(const Pt4Player* player, int voice);

// `order` 0 .. pt4_order_count - 1, `voice` 0..3, `row` 0 .. PT4_ROWS - 1.
Pt4Cell pt4_pattern_cell(const Pt4Player* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
Pt4Player* pt4_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
