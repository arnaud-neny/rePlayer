// SPDX-License-Identifier: MIT
// GlueMon: the module (the file as the replayer reads it) and its typed readers.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/gluemon.h>

// Entries of the period table; a note byte indexes it with its low bit dropped. The replayer reads past it into its own
// variables, which is not modelled: the probe refuses songs that can get there.
#define GM_PERIOD_COUNT 49

// Row entries: a note byte, or one of these. The fourth voice of a percussion song takes command bytes instead.
#define GM_NOTE_OFF 0xFEu
#define GM_NOTE_NONE 0xFFu
// The end of the position table.
#define GM_ORDER_END 0xFFu
#define GM_COMMAND_FIRST 0xC8u
#define GM_COMMAND_LAST 0xCFu

// The fourth voice: it plays percussion commands in a percussion song.
#define GM_PERCUSSION_VOICE (GM_VOICES - 1)

// The patterns the replayer's pointer table holds.
#define GM_MAX_PATTERNS 64

// The replayer finds the first pattern at (position table size + 0x97) & 0xFF from its base: a larger table wraps it
// into the header, which is not modelled.
#define GM_MAX_ORDER_BYTES 0x68

// The waveform of a voice: 16 words.
#define GM_WAVE_BYTES 32u
#define GM_WAVE_WORDS 16u

// Checks the song against the bounds this port needs and fills `module`. `bytes` stays owned by the caller and must
// outlive the module.
bool gm_module_parse(GmModule* module, const uint8_t* bytes, size_t size);

// The waveform of `voice` as the module holds it.
const int8_t* gm_wave(const GmModule* module, uint32_t voice);

// Position table entry `index`: a pattern number, or 0xFF for the end of the table.
uint8_t gm_order(const GmModule* module, uint32_t index);

// The four entries of a row, one per voice.
typedef struct GmRow {
    uint8_t entry[GM_VOICES];
} GmRow;

GmRow gm_row(const GmModule* module, uint8_t pattern, uint32_t row);

// The first 32 bytes of a pattern: what an unknown percussion byte plays as a waveform.
const int8_t* gm_pattern_wave(const GmModule* module, uint8_t pattern);

// A note byte indexes the period table with its low bit dropped.
uint32_t gm_note_index(uint8_t note);

// The period of a note byte.
uint16_t gm_period(uint8_t note);

// A percussion command byte (0xC8..0xCF).
bool gm_is_command(uint8_t entry);
