// SPDX-License-Identifier: MIT
// Laxity (Powertracker) player, translated from the UADE eagleplayer "Laxity-Player" (Eagleplayer V2.1, 1997).

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

#define LX_VOICES 4

// The CIA clock: the timebase of LX_TICK_LENGTH.
#define LX_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / LX_TIMEBASE_HZ seconds.
#define LX_TICK_LENGTH 14187

#define LX_NAME_SIZE 16
#define LX_MAX_INSTRUMENT 63 // the cell holds a 6-bit instrument number

typedef struct LxPlayer LxPlayer;

typedef struct LxTickResult {
    bool looped; // the position list wrapped to its loop position during this tick (the eagleplayer's SongEnd)
} LxTickResult;

typedef struct LxPosition {
    int order; // position in the lists
    int row;
    int rows;  // rows in a pattern
    int speed; // ticks per row
} LxPosition;

typedef struct LxVoiceState {
    int note;       // 1-based note of the last note started, 0 if none
    int instrument; // 1-based instrument of the last note started, 0 if none
} LxVoiceState;

typedef enum LxEffect {
    LxEffect_Arpeggio = 0,
    LxEffect_SlideUp = 1,
    LxEffect_SlideDown = 2,
    LxEffect_TonePortamento = 3,
    LxEffect_Vibrato = 4,
    LxEffect_TimedVolumeSlide = 5,
    LxEffect_Retrigger = 6,
    LxEffect_TickMultiplier = 7,
    LxEffect_VolumeSlide = 10,
    LxEffect_PositionJump = 11,
    LxEffect_SetVolume = 12,
    LxEffect_PatternBreak = 13,
    LxEffect_Filter = 14,
    LxEffect_SetSpeed = 15,
} LxEffect;

// What the cell's volume byte holds: bit 7 clear sets the voice volume, 0x8x / 0x9x slide it up / down by x on every
// tick, 0xFE breaks the pattern; every other value does nothing.
typedef enum LxVolumeColumn {
    LxVolumeColumn_None = 0,
    LxVolumeColumn_Set,
    LxVolumeColumn_SlideUp,
    LxVolumeColumn_SlideDown,
    LxVolumeColumn_PatternBreak,
} LxVolumeColumn;

// One pattern cell as stored.
typedef struct LxCell {
    uint8_t note;       // 1..60 period table index (up to 63 reads past it), 0 none
    uint8_t instrument; // 1-based, 0 none
    LxVolumeColumn volume_kind;
    uint8_t volume_amount; // the volume for Set, the step for the slides
    uint8_t volume_byte;   // as stored
    uint8_t effect;        // LxEffect
    uint8_t param;
} LxCell;

typedef struct LxHeader {
    uint16_t pattern_base;     // offset of pattern 0
    uint16_t instrument_bytes; // size of the instrument block without the 8 bytes of the pattern size behind it
    uint16_t instrument_count; // instruments 1 .. count (the last record is cut to its 8 parameter bytes)
    uint16_t steps;            // positions per voice
    uint16_t loop_step;
    uint16_t pattern_size; // bytes
    uint32_t orders_start; // the four position lists, one after the other
    uint32_t orders_end;
    int64_t sample_offset[LX_MAX_INSTRUMENT + 1]; // by instrument number; OFFSET_ZERO_WORD past the table
} LxHeader;

// The pulse-width synth state of one voice (the replayer's 10-byte record).
typedef struct LxSynthWave {
    bool active; // a synth instrument has been started on the voice
    uint32_t offset;
    int8_t counter;
    uint8_t rate;
    int8_t step;
    uint8_t width;
    int8_t width_min;
    int8_t width_max;
} LxSynthWave;

typedef struct LxModule {
    uint8_t* image; // the file padded with zeros up to the synth buffers; the player writes the waves into it
    uint8_t* zeros; // chip memory for pointers outside the image
    uint32_t image_size;
    uint32_t initial_wave_offset; // the buffers the voices' initial sample pointers name
    LxHeader header;
    char title[LX_NAME_SIZE + 1];
    char names[LX_MAX_INSTRUMENT + 1][LX_NAME_SIZE + 1];
} LxModule;

typedef struct LxVoice {
    LxCell cell;
    int64_t sample;  // module offsets
    uint16_t length; // words
    int64_t loop;
    uint16_t loop_length; // words
    uint16_t period;
    int8_t volume;
    uint8_t vibrato; // speed (high nibble) and depth; the set-volume effect writes here as well
    uint8_t vibrato_pos;
    uint8_t note_index; // the last note * 2
    bool porta_down;    // the tone portamento is running: the period decreases
    int16_t finetune_offset;
    bool start_dma; // the voice starts a sound at the end of the row
    LxSynthWave synth;
    int note;       // last note started, for the display
    int instrument; // last instrument started
    NuadePaulaChannel paula;
} LxVoice;

typedef struct LxSong {
    uint16_t speed;
    int16_t counter; // ticks left in the row
    uint16_t position;
    uint16_t row_offset; // bytes into the pattern
    uint8_t break_count; // pattern breaks and jumps of the row; a jump (0xFF) and a break (+1) in one row wrap it to 0
    uint8_t slide_counter;
    LxVoice voices[LX_VOICES];
    bool looped;
    int shown_order;
    int shown_row;
} LxSong;

struct LxPlayer {
    LxModule module;
    LxSong song;
};

// The eagleplayer's check (two marker bytes and the default title) plus the structure the replayer needs: the header
// tables fit the file, every pattern reachable from the position lists lies inside it, no cell names an instrument
// past the table or an arpeggio note past the period table.
bool lx_probe(const uint8_t* data, size_t size);

// Null when the data is not a module lx_probe accepts, or out of memory.
LxPlayer* lx_create(const uint8_t* data, size_t size);
void lx_destroy(LxPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool lx_start(LxPlayer* player, NuadeVoices* voices);

LxTickResult lx_tick(LxPlayer* player, NuadeVoices* voices);

// The module title (16 bytes of the header, trimmed); instrument names are 1-based like the cells, "" when the
// instrument has no name record.
const char* lx_title(const LxPlayer* player);
const char* lx_instrument_name(const LxPlayer* player, int instrument);
int lx_instrument_count(const LxPlayer* player);

// The row decoded by the last row tick (position 0, row 0 before the first one).
LxPosition lx_position(const LxPlayer* player);
int lx_order_count(const LxPlayer* player);

LxVoiceState lx_voice_state(const LxPlayer* player, int voice);

// `order` 0 .. lx_order_count - 1, `voice` 0..3, `row` 0 .. rows - 1. An empty list entry (>= 0xFE) reads as an empty
// cell.
LxCell lx_pattern_cell(const LxPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
LxPlayer* lx_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
