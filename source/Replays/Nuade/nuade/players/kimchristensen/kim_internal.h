// SPDX-License-Identifier: MIT
// Kim Christensen: the song data and the typed readers of its records. A pointer of the song is an address of the
// game it comes from; the eagleplayer subtracts the address of the data's first byte, so every pointer here is an
// offset into the data, computed with 32-bit wrap-around as the 68000 does.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/kimchristensen.h>

#define KIM_ROWS 16

// The period table has the notes 0 (no note) to KIM_LAST_NOTE; the replayer reads past it into its own variables,
// which the port does not model.
#define KIM_LAST_NOTE 42u

// A volume ramp: the volume (0..64) it reaches after `ticks` ticks.
typedef struct KimVolumeStage {
    uint8_t target;
    uint8_t ticks;
} KimVolumeStage;

// A pitch slide: wait `hold` ticks, then slide to the note `note_offset` semitones from the current one in `ramp`
// ticks.
typedef struct KimPitchStage {
    uint8_t hold;
    uint8_t ramp;
    int8_t note_offset; // semitones from the note before; the note is a byte, so it wraps
} KimPitchStage;

typedef struct KimInstrument {
    KimVolumeStage volume[KIM_VOLUME_STAGES];
    KimPitchStage pitch[KIM_PITCH_STAGES];
    uint8_t vibrato_delay;
    uint8_t vibrato_rate; // ticks of a full cycle
    int16_t vibrato_step; // added to the period every tick of a half cycle
    bool loops;
    uint8_t sample;
    uint32_t start; // added to the sample's address
    uint16_t length_words;
    uint32_t loop_start; // added to the sample's address
    uint16_t loop_words;
} KimInstrument;

typedef struct KimCell {
    uint8_t note; // 0 = none
    uint8_t instrument;
} KimCell;

// Check2 and InitPlayer of the eagleplayer. False when either refuses the data or it leaves the model.
bool kim_module_parse(KimModule* module, const uint8_t* data, size_t size);

// The order list byte `index` bytes after the list's start; false outside the data.
bool kim_order_byte(const KimModule* module, uint32_t index, uint8_t* out);

bool kim_pattern_cell(const KimModule* module, uint8_t pattern, uint32_t voice, uint32_t row, KimCell* out);
bool kim_instrument(const KimModule* module, uint8_t number, KimInstrument* out);

// The offset of the first byte of a sample, from the sample table.
bool kim_sample_offset(const KimModule* module, uint8_t number, uint32_t* out);

// The period of a note; false past the end of the period table.
bool kim_period(uint8_t note, uint16_t* out);

// Whether `words` words (0 = 65536) at `offset` lie inside the data.
bool kim_range_valid(const KimModule* module, uint32_t offset, uint32_t words);
