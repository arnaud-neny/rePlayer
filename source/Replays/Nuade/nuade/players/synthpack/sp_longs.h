// SPDX-License-Identifier: MIT
// Synth Pack, the long layout: instruments are addressed by a table of 32-bit offsets in the sample set, and the notes
// of a pattern column are a list of events.
//
// Song: positions at 0x0C, pattern lines at 0x10C, then a table of 16-bit offsets of the note lists at 0x50C (the
// offsets are from 0x50C). A note list is 6 byte events, each starting at its row: note, volume, instrument, effect,
// row, effect parameter.
// Sample set: "OBISYNTHPACK", a table of 64 offsets of the instruments at 0x0C. An instrument is a 0x30 byte
// header followed by its waveform.

#pragma once

#include "sp_internal.h"

#define SP_LONG_LFO_TABLES 6
#define SP_LONG_LFO_LENGTH 128

extern const int8_t sp_long_lfo_tables[SP_LONG_LFO_TABLES][SP_LONG_LFO_LENGTH];

// The rules of the open-time proof (sp_validate).
extern const SpRules sp_long_rules;

void sp_long_start(SpLongState* state, const SpMemory* song, const SpMemory* set, SpHardware* hardware, int subsong);
void sp_long_tick(SpLongState* state, SpHardware* hardware);

// Every voice has died away.
bool sp_long_silent(const SpLongState* state);

// The voice that drives Paula channel `channel`.
const SpVoice* sp_long_channel_voice(const SpLongState* state, unsigned channel);
