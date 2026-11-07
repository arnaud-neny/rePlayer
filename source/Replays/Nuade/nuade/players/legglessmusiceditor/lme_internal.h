// SPDX-License-Identifier: MIT
// Leggless Music Editor: the module (the file as the replayer reads it) and its typed readers.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/legglessmusiceditor.h>

// The period table: three octaves and the note above (ProTracker periods 856 .. 107).
#define LME_NOTE_COUNT 37

// The instrument number is multiplied by the record size in 16 bits (a signed displacement), so only the first 565
// records can be selected.
#define LME_MAX_INSTRUMENTS 565
// The pattern number is multiplied by 4 in 16 bits (a signed displacement): only the first 0x2000 patterns are reached.
#define LME_MAX_PATTERNS 0x2000

// The vibrato's direction word: the replayer toggles bit 0 of its high byte.
#define LME_VIBRATO_DOWN 0x0100u

// An instrument record, 0x3A bytes.
typedef struct LmeInstrument {
    uint32_t sample_offset; // from the sample base
    uint16_t length_words;
    uint16_t volume;
    uint32_t loop_offset; // from the sample base
    uint16_t loop_words;
    LmeVibrato vibrato;
    LmeEnvelope envelope;
    LmeWave wave;
    LmeRetrigger retrigger;
} LmeInstrument;

// Checks the header (the eagleplayer's Check2 and InitPlayer). `bytes` stays owned by the caller and must outlive the
// module.
bool lme_module_parse(LmeModule* module, const uint8_t* bytes, size_t size);

// Walks every subsong's position lists and patterns once and checks everything the replayer reads from them: patterns,
// notes, instruments, samples. Sets `grid_ticks`.
bool lme_module_validate(LmeModule* module);

// Where the position list of `voice` in `subsong` starts (a file offset).
uint32_t lme_position_list(const LmeModule* module, uint32_t subsong, uint32_t voice);

// A position list entry: a pattern with an optional transposition, or the end mark.
typedef struct LmePosition {
    bool end;
    uint32_t pattern_start; // file offset of the pattern's first word
    int16_t transpose;      // added to the pattern's notes
} LmePosition;

// Reads the entry at `*list_pos` and advances. False when it runs out of the file or names a pattern that does not
// exist.
bool lme_read_position(const LmeModule* module, uint32_t* list_pos, LmePosition* position);

// One event of a pattern. Words are read in this order: flags, then instrument number (instrument), arpeggio, slide
// target, then duration and note.
typedef struct LmeArpeggio {
    uint16_t count; // slots, including the note's own period
    uint16_t reload;
    uint16_t notes[LME_ARPEGGIO_SLOTS - 1];
} LmeArpeggio;

typedef struct LmeSlide {
    uint16_t note;
    int16_t steps;
    uint16_t delay;
} LmeSlide;

typedef struct LmeEvent {
    bool stop; // switch the voice's DMA off; only the duration follows
    bool legato;
    bool has_instrument;
    bool has_arpeggio;
    bool has_slide;
    uint16_t instrument;
    LmeArpeggio arpeggio;
    LmeSlide slide;
    uint16_t duration;
    uint16_t note;
} LmeEvent;

typedef enum LmeRead {
    LmeRead_Event = 0,
    LmeRead_PatternEnd, // the flags word had its top bit set: the pattern is over
    LmeRead_Invalid,    // the pattern runs out of the file or has an arpeggio the replayer cannot play
} LmeRead;

// Reads the event at `*pos` and advances. The first word of a pattern is taken as flags even with its top bit set.
LmeRead lme_read_event(const LmeModule* module, uint32_t* pos, bool first_of_pattern, LmeEvent* event);

// The period of a note after transposition; false when it is outside the period table.
bool lme_note_period(uint16_t note, int16_t transpose, uint16_t* period);

// Record `index` (below instrument_count).
LmeInstrument lme_instrument(const LmeModule* module, uint32_t index);

// The sample bytes of an instrument start (or loop) at this offset from the sample base; AUDxLC bit 0 does not exist,
// so an odd offset reads from the even address below it.
const int8_t* lme_sample_at(const LmeModule* module, uint32_t offset);

// The 68000 test `cmp.w limit, value; bpl` (the sign of the 16-bit difference).
static inline bool lme_difference_not_negative(int16_t value, int16_t limit) {
    return (int16_t)(value - limit) >= 0;
}

// One step of a synth wave's pulse width: the new type and width.
void lme_wave_step(LmeWave* wave);
