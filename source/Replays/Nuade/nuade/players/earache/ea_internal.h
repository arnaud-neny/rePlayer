// SPDX-License-Identifier: MIT
// EarAche: the module (the file as the replayer reads it) and its typed readers.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/earache.h>

// Entries of the period table (ea_period_entry); the replayer indexes past it into its own variables and code, which
// is not modelled: the probe refuses songs that can get there.
#define EA_PERIOD_COUNT 71

// Position table entries: a word of 0xFFFF starts a jump (the next word, & 0xFFF, is the entry to continue at).
#define EA_JUMP_MARK 0xFFFF

// Locates the header (the eagleplayer's Check2) and checks the song against the bounds this port needs. `bytes` stays
// owned by the caller and must outlive the module.
bool ea_module_parse(EaModule* module, const uint8_t* bytes, size_t size);

// A byte `offset` from the header start; 0 past the end of the file.
uint8_t ea_byte(const EaModule* module, uint64_t offset);

typedef struct EaPositionEntry {
    uint16_t first;  // first row of the pattern, or EA_JUMP_MARK
    uint16_t second; // last row | speed << 12, or the entry to jump to
} EaPositionEntry;

EaPositionEntry ea_position_entry(const EaModule* module, uint32_t index);

// The ticks per row a (pattern) entry sets: the top 4 bits of its second word.
uint8_t ea_entry_speed(EaPositionEntry entry);

// The pattern the sequencer plays from position table entry `index`: a jump mark continues at the entry its second word
// names, following up to 16 marks in a row (the eagleplayer calls SongEnd at the first).
typedef struct EaPattern {
    uint16_t first_row;
    uint16_t last_row;
    uint8_t speed;       // ticks per row
    uint32_t next_index; // the entry after the pattern's
    bool jumped;         // a jump mark was followed
} EaPattern;

// False when 256 fetches (each following up to 16 marks in a row) lead to no pattern.
bool ea_pattern_at(const EaModule* module, uint32_t index, EaPattern* pattern);

// The word `voice` plays in `row`; zero means no note.
typedef struct EaCell {
    uint8_t note;
    uint8_t instrument;
} EaCell;

EaCell ea_cell(const EaModule* module, uint32_t row, uint32_t voice);

typedef struct EaInstrument {
    uint8_t sample;        // index in the sample table (7 bits)
    uint8_t gate_ticks;    // 0 plays the sample once, 255 loops forever, else loops this many ticks
    uint32_t volume_table; // offset of its volume table
    bool arpeggio;         // the pitch byte had its top bit set: the pitch table is an arpeggio
    uint8_t arpeggio_step; // third arpeggio offset (the low 7 bits of the pitch byte)
    uint32_t pitch_table;  // offset of its pitch table (not an arpeggio)
    uint8_t volume_ticks;  // ticks until the first volume table entry is read
    uint8_t volume_reload; // ticks between volume table entries
    uint8_t pitch_ticks;   // ticks until the first pitch step (also the first arpeggio offset)
    uint8_t pitch_reload;  // ticks between pitch steps (also the second arpeggio offset)
    uint8_t volume;        // volume at the note start
    int8_t fine_tune;      // added to the note's period, shifted left by 5 minus the octave (mod 8)
} EaInstrument;

EaInstrument ea_instrument(const EaModule* module, uint32_t index);

typedef struct EaSample {
    const int8_t* data;    // null when the entry or the sample start is not inside the file
    uint16_t length_words; // AUDxLEN as the table has it: 0 plays 65536 words
} EaSample;

EaSample ea_sample(const EaModule* module, uint32_t index);

uint16_t ea_period_entry(uint32_t index);
