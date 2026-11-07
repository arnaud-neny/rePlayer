// SPDX-License-Identifier: MIT
// SynTracker player (1993), translated from the UADE eagleplayer "Syntracker V1.70 Replayer-Module" (Wanted Team).

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

#define STK_VOICES 4
#define STK_ROWS 32
#define STK_INSTRUMENTS 32
// Instruments 1 .. STK_SYNTH_INSTRUMENTS are synthetic (a 32 byte wave, an envelope, a filter and a morph); the rest
// are sampled.
#define STK_SYNTH_INSTRUMENTS 16
#define STK_NAME_SIZE 32

// The CIA clock: the timebase of STK_TICK_LENGTH.
#define STK_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / STK_TIMEBASE_HZ seconds.
#define STK_TICK_LENGTH 14187

#define STK_PHASES 4 // envelope and filter: four (duration, amount) pairs
#define STK_WAVE_SIZE 32
#define STK_SLOT_WORK 1
#define STK_SLOT_PLAY (STK_SLOT_WORK + STK_VOICES)
#define STK_SLOT_COUNT (STK_SLOT_PLAY + STK_VOICES)

typedef struct StkPlayer StkPlayer;

typedef struct StkTickResult {
    bool looped; // the song wrapped from its last position to the first during this tick
} StkTickResult;

typedef struct StkPosition {
    int order; // song position
    int row;
    int speed; // ticks per row
} StkPosition;

typedef struct StkVoiceState {
    int note;       // period table index of the last note started (after the position's transpose), -1 if none
    int instrument; // 1-based instrument of the voice, 0 if none
} StkVoiceState;

// One pattern cell as stored (no transposes applied).
typedef struct StkCell {
    uint8_t note;       // period table index + 1, 0 none
    uint8_t instrument; // 1 .. 32; 0 repeats the voice's instrument, 0xFF keeps the playing one, 0x40 .. 0x4F swap
                        // the synth wave of the playing note
    uint8_t command;    // ASCII letter of the effect, 0 none
    uint8_t param;
} StkCell;

typedef struct StkModule {
    uint8_t* data; // the file in a nuade_paula_padded_copy: a sample may run past its end into zeros
    uint32_t size; // of the file
    uint16_t song_length;
    uint32_t instruments_base;
    uint32_t orders_base;
    uint32_t patterns_base;
    uint32_t sample_base; // the sample data follows the patterns
} StkModule;

typedef struct StkPhase {
    uint8_t duration; // ticks
    uint8_t amount;   // the envelope's rate, the filter's weight
} StkPhase;

// A synth instrument's parameters.
typedef struct StkSynthParams {
    StkPhase envelope[STK_PHASES];
    StkPhase filter[STK_PHASES];
    uint8_t vibrato; // speed << 4 | depth
    uint8_t morph_speed;
    uint8_t vibrato_wave; // block number of the vibrato table
    uint8_t morph_start;  // the morph position at the note start, and where the sweep turns
    uint8_t morph_end;
    int8_t morph_jump;
    uint8_t morph_wave; // block number of the wave that is mixed over the base wave
} StkSynthParams;

// AUDxLC / AUDxLEN queued for the end of the tick; len 0 means nothing queued.
typedef struct StkWaveLoad {
    const int8_t* lc;
    uint16_t len;
} StkWaveLoad;

typedef struct StkVoice {
    uint16_t volume;            // the volume the envelope scales
    uint16_t instrument_volume; // the instrument's volume, 0..64
    uint16_t vibrato_depth;
    uint16_t vibrato_speed;
    const int8_t* vibrato_table;
    uint16_t vibrato_pos;
    bool synth_active; // a synth instrument plays (sample voices have no wave of their own)
    StkSynthParams synth;
    const int8_t* base_wave; // the synth instrument's wave
    uint16_t envelope_phase; // STK_PHASES once the four phases ran
    uint8_t envelope_ticks;
    uint8_t envelope_level;
    uint8_t instrument;
    uint16_t periods[3]; // the arpeggio's notes: the note, then the two arpeggio notes
    uint16_t arpeggio_phase;
    uint8_t arpeggio_speed;
    uint8_t arpeggio_ticks;
    uint16_t period; // the arpeggio note that sounds
    uint16_t note_index;
    uint16_t slide_offset; // added to the period; the slide effects move it
    uint16_t filter_phase;
    uint8_t filter_ticks;
    int8_t morph_step;
    uint8_t morph_speed;
    uint8_t morph_ticks;
    uint8_t morph_pos;
    bool new_note;      // the working wave is the instrument's own at the next tick
    uint16_t decay;     // volume taken off every tick
    int16_t slide;      // period added to slide_offset every tick
    StkWaveLoad start;  // for the DMA start
    StkWaveLoad repeat; // and for the repeat

    int shown_note;
    int shown_instrument;

    NuadePaulaChannel paula;
} StkVoice;

typedef struct StkChipRam {
    int8_t slots[STK_SLOT_COUNT][STK_WAVE_SIZE];
} StkChipRam;

typedef struct StkSong {
    uint16_t speed;
    uint16_t tick;
    uint16_t position;
    uint16_t row;
    uint16_t jump;                 // the position to go to when the next row starts, NO_JUMP for none
    uint16_t portamento_remainder; // the remainder of the last portamento division of this row
    StkVoice voices[STK_VOICES];
    StkChipRam ram;
    bool looped;
    int shown_position;
    int shown_row;
} StkSong;

struct StkPlayer {
    StkModule module;
    StkSong song;
    char names[STK_INSTRUMENTS][STK_NAME_SIZE + 1];
};

// The eagleplayer's check: the file starts with "SYNTRACKER-SONG:".
bool stk_probe(const uint8_t* data, size_t size);

// Null when the data is not a SynTracker module.
StkPlayer* stk_create(const uint8_t* data, size_t size);
void stk_destroy(StkPlayer* player);

// (Re)start the song from its beginning (InitSound). stk_tick drives `voices` (Paula channel n = voice n).
void stk_start(StkPlayer* player);

StkTickResult stk_tick(StkPlayer* player, NuadeVoices* voices);

// 0-based; the 32 byte name field of the instrument (the first ones usually hold the song's text), "" when empty.
const char* stk_instrument_name(const StkPlayer* player, int instrument);

// The row decoded by the last row tick (position 0, row 0 before the first one).
StkPosition stk_position(const StkPlayer* player);
int stk_order_count(const StkPlayer* player);

StkVoiceState stk_voice_state(const StkPlayer* player, int voice);

// `order` 0 .. stk_order_count - 1, `voice` 0..3, `row` 0 .. STK_ROWS - 1.
StkCell stk_pattern_cell(const StkPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
StkPlayer* stk_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
