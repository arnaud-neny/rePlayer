// SPDX-License-Identifier: MIT
// TimeTracker player (BrainWasher & FireBlade, 1993), translated from the UADE eagleplayer (Wanted Team).

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

#define TT_VOICES 4
#define TT_ROWS 64

// The CIA clock: the timebase of TT_TICK_LENGTH.
#define TT_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / TT_TIMEBASE_HZ seconds.
#define TT_TICK_LENGTH 14187

#define TT_MAX_SAMPLES 31

typedef struct TtPlayer TtPlayer;

typedef enum TtStatus {
    TtStatus_Ok = 0,
    TtStatus_InvalidData,
    TtStatus_MissingSamples, // the sample file (SMP.<name> for TMK.<name>) is not there; UADE refuses the song too
    TtStatus_OutOfMemory,
} TtStatus;

typedef struct TtCreateResult {
    TtPlayer* player; // 0 on failure
    TtStatus status;
    char missing_file[128]; // the sample file name, for TtStatus_MissingSamples
} TtCreateResult;

typedef struct TtTickResult {
    bool looped; // the replayer called the host's SongEnd during this tick (the song wrapped, or a position jump)
} TtTickResult;

typedef struct TtPosition {
    int order; // song position
    int row;
    int speed; // ticks per row
} TtPosition;

typedef struct TtVoiceState {
    int note;   // period table index of the last note started, 0 if none (1 = C-1)
    int sample; // 1-based sample number last selected on the voice, 0 if none
} TtVoiceState;

// One pattern cell as stored.
typedef struct TtCell {
    uint8_t note;   // 1..63 period table index (1 = C-1), 0 none
    uint8_t sample; // 1-based, 0 none
    uint8_t cmd;    // ProTracker effect
    uint8_t param;
} TtCell;

typedef struct TtVoice {
    uint16_t note_period; // +0x00 the period of the note of this row, 0 if none
    uint8_t cmd;          // +0x02 the effect of this row
    uint8_t param;        // +0x03
    const int8_t* sample; // +0x04 the sample start (moved by the sample offset effect)
    uint16_t length;      // +0x08 words played at a DMA start
    const int8_t* loop;   // +0x0A
    uint16_t loop_length; // +0x0E words
    uint16_t period;      // +0x10 the note's period (finetuned), the base of the pitch effects
    uint8_t finetune;     // +0x12
    uint8_t volume;       // +0x13
    // +0x14 the voice's DMACON bit
    bool porta_down;        // +0x16 the tone portamento slides the period down
    uint8_t porta_speed;    // +0x17
    uint16_t porta_target;  // +0x18
    uint8_t vibrato;        // +0x1A speed (high nibble) and depth (low nibble)
    uint8_t vibrato_pos;    // +0x1B
    uint8_t tremolo;        // +0x1C
    uint8_t tremolo_pos;    // +0x1D
    uint8_t waveform;       // +0x1E tremolo waveform (high nibble), vibrato waveform (low nibble, never set)
    uint8_t glissando_funk; // +0x1F funk speed (high nibble), glissando (low nibble)
    uint8_t offset_memory;  // +0x20 the last 9xx parameter
    uint8_t loop_row;       // +0x21 E60: the row the pattern loop returns to
    uint8_t loop_count;     // +0x22
    uint8_t funk_acc;       // +0x23
    const int8_t* funk_pos; // +0x24 the loop start EFx has moved on to
    uint16_t full_length;   // +0x28 the sample's length in words

    bool started; // got a note on this row: its DMA starts after the row
    uint8_t note; // for the display: the note last started
    uint8_t sample_number;

    NuadePaulaChannel paula;
} TtVoice;

// One subsong's block of the module.
typedef struct TtBlock {
    uint8_t order_count;
    uint32_t patterns; // the position list
    uint32_t refs;     // the reference table
} TtBlock;

typedef struct TtModule {
    uint8_t* data;
    uint32_t size;
    int subsong_count;
    uint32_t first_block;
    uint32_t sample_table; // of the first block, which every subsong uses
    int sample_count;
    bool delta;       // the samples are delta coded
    uint8_t* samples; // the sample file, delta decoded, in a nuade_paula_padded_copy (see prepare_samples)
    uint32_t sample_offset[TT_MAX_SAMPLES];
} TtModule;

typedef struct TtSong {
    TtBlock block;
    uint8_t row;
    uint8_t speed;
    uint8_t tick;
    uint8_t position;
    bool break_flag;   // Dxx / Bxx: the next row is jump_row of the next position
    bool jump_flag;    // E6x: the next row is jump_row of this pattern
    uint8_t jump_row;  // the row to continue at
    uint8_t fine_mask; // E1x / E2x take the low nibble of the parameter. The replayer starts it at 0, so its first
                       // 1xx / 2xx slides by nothing
    uint8_t delay_set; // EEx
    uint8_t delay_count;
    TtVoice voices[TT_VOICES];
    bool looped;
    int shown_position;
    int shown_row;
} TtSong;

struct TtPlayer {
    TtModule module;
    TtSong song;
};

// The eagleplayer's check: "TMK" and a non-zero subsong count.
bool tt_probe(const uint8_t* data, size_t size);

// Loads the module and its sample file, which sits next to it: "TMK.<name>" is paired with "SMP.<name>", a name
// ending in ".pat" with "<name>.SAM" (any case).
TtCreateResult tt_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void tt_destroy(TtPlayer* player);

int tt_subsong_count(const TtPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool tt_start(TtPlayer* player, int subsong, NuadeVoices* voices);

TtTickResult tt_tick(TtPlayer* player, NuadeVoices* voices);

// The row decoded by the last row tick (position 0, row 0 before the first one).
TtPosition tt_position(const TtPlayer* player);
// Positions of the started subsong.
int tt_order_count(const TtPlayer* player);

TtVoiceState tt_voice_state(const TtPlayer* player, int voice);

// `order` 0 .. tt_order_count - 1, `voice` 0..3, `row` 0 .. TT_ROWS - 1.
TtCell tt_pattern_cell(const TtPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
TtPlayer* tt_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
