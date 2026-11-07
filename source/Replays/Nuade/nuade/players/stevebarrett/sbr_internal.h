// SPDX-License-Identifier: MIT
// Steve Barrett: types shared by the module check and the replayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/players/stevebarrett.h>

// The module is a 68k program: four `bra.w` entries (init, song select, tick, stop), the game's title, the driver, its
// variables and tables, then the IFF 8SVX samples. The driver's register a4 points six bytes before the module: every
// song address in the tables is a 16-bit offset from a4.

#define SBR_A4_BIAS 6
#define SBR_WAVE_BYTES 8 // a record of the wave table
#define SBR_ENV_BYTES 8  // a record of the envelope table
#define SBR_INSTRUMENT_BYTES 6
#define SBR_RECORD_BYTES 0x80 // the four voice records, field by field: field $f of voice v is at $f + v

// UADE loads the module at this address. Only a note number far past the period table can see it: the table that
// follows holds the samples' addresses.
#define SBR_LOAD_ADDRESS 0xf000u

// The module's byte offset of an address given as a 16-bit offset from a4.

static inline int64_t sbr_offset(int32_t from_a4) {
    return (int64_t)from_a4 - SBR_A4_BIAS;
}

// The module check and the proof that the songs stay inside the port, on the copy of the module that the driver's init
// patches (owned by `m` once this returns Ok).
SbrStatus sbr_module_open(SbrModule* m, const uint8_t* data, size_t size);
void sbr_module_close(SbrModule* m);

// The module's records.

uint8_t sbr_module_byte(const SbrModule* m, int64_t offset); // 0 outside the module

// A record of the wave table. `offset` is the record's byte offset in the table: the sample number, and two words that
// the build's loop style reads as the repeat's start and length.
typedef struct SbrWave {
    uint8_t instrument;
    uint16_t loop_start;
    uint16_t loop_length; // bytes, or words for the build with SbrLoop_VoiceZero
} SbrWave;

// False when the record is not inside the module.
bool sbr_module_wave(const SbrModule* m, uint8_t offset, SbrWave* out);

typedef struct SbrEnv {
    uint8_t volume;
    uint8_t target;
    uint8_t step;
    uint8_t release;
    uint8_t sustain_shift;
    uint8_t vibrato;
    uint8_t chord;
    uint8_t flags;
} SbrEnv;
// False when the record is not inside the module.
bool sbr_module_env(const SbrModule* m, uint8_t offset, SbrEnv* out);

// The list of a voice in a song, as an offset from a4.
int32_t sbr_module_list(const SbrModule* m, uint32_t song, uint32_t voice);

// The start of a pattern, as an offset from a4.
uint16_t sbr_module_pattern(const SbrModule* m, uint8_t index);

uint8_t sbr_module_chord(const SbrModule* m, unsigned bit);

// A word of the period table by index. The driver reads past the table for a note number past it, into whatever
// follows: that is the table's own padding (a constant), the four current periods, or the sample table.
typedef enum SbrWordKind {
    SbrWord_Image,      // a constant of the module
    SbrWord_Live,       // the current period of a voice
    SbrWord_Record,     // two bytes of the voice records
    SbrWord_Instrument, // a word of the sample table
    SbrWord_Unmodelled, // a variable the port does not model
} SbrWordKind;
SbrWordKind sbr_module_period_kind(const SbrModule* m, unsigned index);

// The word of an Image or Instrument index.
uint16_t sbr_module_period_word(const SbrModule* m, unsigned index);

// The voice whose period a Live index is.
unsigned sbr_module_period_voice(const SbrModule* m, unsigned index);

// The byte offset in the voice records of a Record index.
unsigned sbr_module_period_record(const SbrModule* m, unsigned index);

// The pattern byte code. A note is some prefixes and a note byte; the driver reads them in this order.

typedef enum SbrEventKind {
    SbrEvent_Note,
    SbrEvent_PatternEnd, // an $ff: the next pattern of the list
    SbrEvent_Stop,       // an $fe: the song stops
    SbrEvent_Truncated,  // the pattern runs out of the module
} SbrEventKind;

typedef enum SbrCommand {
    SbrCommand_None,
    SbrCommand_VibratoDelay, // $7a, one parameter
    SbrCommand_Chord,        // $7d, one parameter: the chord's notes, for this note
    SbrCommand_ChordHold,    // $7c, the same, for the next notes too
    SbrCommand_Unmodelled,   // $7b, $7e, $7f: slides and glides, which the port does not model
} SbrCommand;

typedef struct SbrEvent {
    SbrEventKind kind;
    bool has_tempo;
    uint8_t tempo;
    bool has_wave;
    uint8_t wave; // a record offset of the wave table
    bool has_env;
    uint8_t env; // a record offset of the envelope table
    bool has_duration;
    uint8_t duration;
    SbrCommand command;
    uint8_t parameter;
    uint8_t note;
    uint32_t consumed; // the bytes read
} SbrEvent;

// Decodes the event at `offset` of the module. The prefixes it passed are set even when the event ends the pattern.
SbrEvent sbr_decode_event(const SbrModule* m, int64_t offset);
