// SPDX-License-Identifier: MIT
// Synth Pack, the word layout: instruments are addressed by a table of 16-bit offsets at the start of the sample set,
// and the notes of a pattern column are blocks of 64 rows.
//
// Song: positions at 0x0C, pattern lines at 0x10C, then the note blocks at 0x50C, 0x180 bytes each: a 6 byte event for
// every row: note (0xFF with volume 0 releases), volume, instrument, effect, effect, effect parameter.
// Sample set: a table of offsets of the instruments from the start of the set; the first offset is the table's end.
// An instrument is a 0x22 byte header followed by its waveform.

#pragma once

#include "sp_internal.h"

#define SP_WORD_LFO_LENGTH 32
#define SP_WORD_VOLUME_LFO_TABLES 3
#define SP_WORD_PITCH_LFO_TABLES 6

extern const uint16_t sp_word_volume_lfo_tables[SP_WORD_VOLUME_LFO_TABLES][SP_WORD_LFO_LENGTH];
extern const int8_t sp_word_pitch_lfo_tables[SP_WORD_PITCH_LFO_TABLES][SP_WORD_LFO_LENGTH];

// The rules of the open-time proof (sp_validate).
extern const SpRules sp_word_rules;

void sp_word_start(SpWordState* state, const SpMemory* song, const SpMemory* set, SpHardware* hardware, int subsong);
void sp_word_tick(SpWordState* state, SpHardware* hardware);

// Every voice has died away.
bool sp_word_silent(const SpWordState* state);

// The voice that drives Paula channel `channel`.
const SpVoice* sp_word_channel_voice(const SpWordState* state, unsigned channel);
