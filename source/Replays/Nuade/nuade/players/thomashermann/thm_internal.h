// SPDX-License-Identifier: MIT
// Thomas Hermann player: the module (file layout, typed readers) and the note set-up that the module loader's proof and
// the player share.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/players/thomashermann.h>

#define THM_INSTRUMENT_SIZE 0x30

// Note bytes from this value on are commands; the notes below it are (note index << 4) | octave class.
#define THM_COMMAND_NOTES 0xc0

typedef enum ThmCommand {
    ThmCommand_Retrigger = 0xcf, // play the instrument again at its own period
    ThmCommand_Release = 0xdf,
    ThmCommand_Stop = 0xef,
    ThmCommand_None = 0xff,
} ThmCommand;

// The instrument's loop flags byte: 0 plays the block twice and stops, a value with bit 7 set runs the envelope, any
// other value loops at constant volume.
#define THM_ENVELOPE_FLAG 0x80

// One entry of the replayer's subsong table.
typedef struct ThmSubsong {
    uint8_t loop_position; // the position after the last one
    uint8_t last_position;
    uint8_t position; // the position the song starts at
    uint8_t speed;    // ticks between the read-ahead and the row, minus one
} ThmSubsong;

typedef struct ThmInstrument {
    uint16_t length; // sample bytes
    uint8_t volume;
    uint8_t type; // 1 selects the note-to-period rules of the first set
    uint32_t sample;
    uint16_t loop_offset;
    uint16_t loop_bytes;
    uint16_t base_period;
    uint8_t flags;       // the loop flags
    uint8_t steps[4];    // envelope: ticks of the attack and of the three following segments
    uint8_t levels[3];   // envelope: the levels the segments reach
    uint8_t slide_speed; // period units per run of the envelope timer, of a sliding note
} ThmInstrument;

// The sample a note starts (the replayer's per-note work record).
typedef struct ThmNote {
    uint16_t length; // bytes
    uint8_t volume;
    uint8_t flags;
    uint64_t sample; // 64 bits: the octave offsets are added to a placement that may be off the files
    uint32_t loop_offset;
    uint16_t loop_bytes;
    uint16_t period;
    uint16_t base_period;
    uint16_t loop_offset_carry; // the bits the shifted loop offset pushes past 16, which the replayer keeps
} ThmNote;

// Parse the module, load the sample file and prove the songs; null (with *status) when it cannot play it.
ThmModule* thm_module_load(const uint8_t* data, size_t size, const char* path, const NuadeIo* io, ThmStatus* status);
void thm_module_free(ThmModule* module);

ThmSubsong thm_module_subsong(const ThmModule* module, uint32_t subsong);
uint8_t thm_module_instrument_count(const ThmModule* module);
uint8_t thm_module_last_row(const ThmModule* module);
uint8_t thm_module_row_count(const ThmModule* module);
uint16_t thm_module_timer(const ThmModule* module);
bool thm_module_voice_off(const ThmModule* module, uint32_t voice);

ThmInstrument thm_module_instrument(const ThmModule* module, uint8_t number);
ThmRow thm_module_row(const ThmModule* module, uint32_t voice, uint8_t position, uint8_t row);
int8_t thm_module_transposition(const ThmModule* module, uint32_t voice, uint8_t position);

// AUDxLC for a chip offset: null past the sample data.
const int8_t* thm_module_chip(const ThmModule* module, uint64_t offset);

uint8_t thm_transpose_note(uint8_t note, int8_t transposition);

// The note's sample and period. False when the replayer starts no note; out->period is then the period it had worked
// out (the table period, shifted for the rules that got that far).
bool thm_note_build(uint8_t note, const ThmInstrument* instrument, ThmNote* out);
