// SPDX-License-Identifier: MIT
// Tronic: the module (the file as the replayer reads it), the voice record and the routines the open-time check runs.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/tronic.h>

#include "nuade_paula_channel.h"

#define TRONIC_ROW_BYTES 4
#define TRONIC_PATTERN_BYTES (TRONIC_ROWS * TRONIC_ROW_BYTES)
#define TRONIC_WAVE_TABLE_BYTES 16
#define TRONIC_SAMPLE_COUNT 8

// Entries of the period table; an index past it reads the replayer's own variables, which is not modelled: the check
// refuses songs that can get there.
#define TRONIC_PERIOD_COUNT 86

#define TRONIC_DEFAULT_SPEED 5
#define TRONIC_FULL_VOLUME 0x3F

// Module

// Locates the data blocks (the eagleplayer's Check2 and InitSound) and checks that they lie inside the file. `data`
// is copied. False if the file is not a Tronic module.
bool tronic_module_parse(TronicModule* module, const uint8_t* data, size_t size);

void tronic_module_free(TronicModule* module);

uint8_t tronic_byte(TronicModule* module, uint64_t at);
uint16_t tronic_word(TronicModule* module, uint64_t at);
uint32_t tronic_long(TronicModule* module, uint64_t at);

// The period of a period-table index; 0 past the table.
uint16_t tronic_period(unsigned index);

// A pattern entry: row `row` of the pattern at `pattern`.
TronicCell tronic_pattern_row(TronicModule* module, uint32_t pattern, unsigned row);

// A step of a voice's sequence: the offset of its pattern in the module and its transpose.
typedef struct TronicStep {
    uint32_t pattern;
    uint8_t transpose;
} TronicStep;

TronicStep tronic_step(TronicModule* module, int voice, unsigned position);

// Instruments

#define TRONIC_WAVE_TABLE_RESTART 0x80

typedef enum TronicEffect {
    TronicEffect_None = 0,
    TronicEffect_Speed,
    TronicEffect_Nothing,
    TronicEffect_SlideUp,
    TronicEffect_SlideDown,
    TronicEffect_SlideToNote,
    TronicEffect_VoiceVolume,
    TronicEffect_MasterVolume,
    TronicEffect_WaveTable,
    TronicEffect_Last = TronicEffect_WaveTable,
} TronicEffect;

typedef enum TronicInstrumentField {
    TronicField_Envelope = 0x06, // stages of 3 bytes: step, target, hold
    TronicField_Vibrato = 0x15,  // stages of 3 bytes: step, ticks per direction, hold
    TronicField_Steps = 0x28,    // waveform list: waveform numbers, 0xFF followed by the position to continue at
} TronicInstrumentField;

// Instrument `number` (1-based); false when it is outside the table or its record outside the file.
bool tronic_instrument(TronicModule* module, unsigned number, TronicInstrument* out);

uint8_t tronic_instrument_byte(TronicModule* module, const TronicInstrument* instrument, uint32_t field);

// One stage of the volume envelope: the level moves by `step` per tick to `target`, then the stage holds for `hold`
// ticks.
typedef struct TronicEnvelopeStage {
    uint8_t step;
    uint8_t target;
    uint8_t hold;
} TronicEnvelopeStage;

// `pos` is the byte offset of the stage in the envelope; the replayer lets it run past the five stages.
TronicEnvelopeStage tronic_envelope_stage(TronicModule* module, const TronicInstrument* instrument, uint16_t pos);

// One stage of the vibrato: the pitch moves by `step` per tick, in turns up and down every `ticks` ticks.
typedef struct TronicVibratoStage {
    uint8_t step;
    uint8_t ticks;
    uint8_t hold;
} TronicVibratoStage;

TronicVibratoStage tronic_vibrato_stage(TronicModule* module, const TronicInstrument* instrument, uint8_t stage);

// Voice

// The memory the voices' repeat registers point at when nothing else was written: zeros behind the module.
const int8_t* tronic_silence(const TronicModule* module);

void tronic_voice_reset(TronicModule* module, TronicVoice* voice, int index);

// Effect-free parts of a voice's work, run by the tick and by the open-time check.
void tronic_start_note(TronicModule* module, TronicVoice* voice, unsigned instrument_number);
// `clear_first_word` is false for the open-time check, which must not change the playback data.
void tronic_sample_repeat(TronicModule* module, TronicVoice* voice, bool clear_first_word);
void tronic_run_waveform_list(TronicModule* module, TronicVoice* voice);
void tronic_run_envelope(TronicModule* module, TronicVoice* voice);
void tronic_run_vibrato(TronicModule* module, TronicVoice* voice);

// The wave table effect 8 selects: after a note the high byte of the instrument offset `2 * number - 2` above the
// argument; without a note the default table (the check admits only argument 0 there).
uint32_t tronic_effect_wave_table(const TronicModule* module, TronicCell row);

// True when the module passes the open-time check (tronic_check.c).
bool tronic_module_prove(TronicModule* module);
