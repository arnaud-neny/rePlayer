// SPDX-License-Identifier: MIT
// Tim Follin player: the module as the eagleplayer relocates it. Internal to players/timfollin. The layout of the song
// table is in tfol_layout.h.

#pragma once

#include "nuade_base.h"
#include <nuade/players/timfollin.h>

// The replayer's constants shared by the replayer and the proof.
// A song has three voices, or four when its voice count says so.
#define TFOL_MIN_VOICES 3
// A fetch that runs more than this many events without ending (a note or a stop) stops the voice; the proof refuses
// a module with such a fetch.
#define TFOL_FETCH_LIMIT 65536
// The sweep: instruments below this number are swept; it moves a position of the sample between SWEEP_FIRST and
// SWEEP_LAST, starting at SWEEP_START; the reset of the sweep clears RESET_FILLED bytes of 0x80 then RESET_ZEROED zeros
// of the instrument record from RESET_FIRST.
#define TFOL_SWEEP_INSTRUMENTS 4
// The sweep position is 0 until the first reset, so the first sweep writes start at TFOL_SWEEP_LOWEST.
#define TFOL_SWEEP_LOWEST 1
#define TFOL_SWEEP_FIRST 4
#define TFOL_SWEEP_LAST 0x1e
#define TFOL_SWEEP_START 7
#define TFOL_RESET_FIRST 2
#define TFOL_RESET_FILLED 8
#define TFOL_RESET_ZEROED 24
// The wave flags byte: bit 7 note off, bit 6 second sample, bits 0..5 the entry of the second sample.
#define TFOL_WAVE_SECOND_SAMPLE 0x40
#define TFOL_WAVE_NOTE_OFF 0x80
#define TFOL_WAVE_SAMPLE_INDEX 0x3f

typedef enum TfolCommand {
    TfolCommand_Nop = 0,
    TfolCommand_Instrument,
    TfolCommand_Call,
    TfolCommand_Return,
    TfolCommand_LoopStart,
    TfolCommand_LoopEnd,
    TfolCommand_Envelope,
    TfolCommand_Portamento,
    TfolCommand_Vibrato,
    TfolCommand_PitchModulation,
    TfolCommand_Transpose,
    TfolCommand_NoteLength,
    TfolCommand_EnvelopeRestart,
    TfolCommand_WaveFlags,
    TfolCommand_SweepRate,
    TfolCommand_Stop,
    TfolCommand_EnvelopeStep,
    TfolCommand_SkipTranspose,
    TfolCommand_Jump,
    TfolCommand_Count,
} TfolCommand;

// One event of a voice's stream: a note with its length, or a command with its arguments.
typedef struct TfolEvent {
    bool valid;          // known command, and every byte and the address it names lie inside the module
    bool is_note;        // else a command
    uint8_t note;        // is_note: the note byte
    TfolCommand command; // !is_note
    uint8_t args[4];     // the argument bytes; a note has its length in args[0] unless the voice has a note length
    uint32_t target;     // Call, Jump: the file offset the address names
    uint32_t start;      // the bytes of the event are [start, next)
    uint32_t next;
} TfolEvent;

// Check2, the relocation of InitPlayer and the open-time proof (tfol_proof.c): fills `module` (without the memory
// copies) when the eagleplayer plays the file and the port models every byte the replayer can reach, false otherwise.
bool tfol_module_locate(const uint8_t* data, size_t size, TfolModule* module);

// The proof: walks every stream of every playable song and refuses the module when the replayer could leave the
// modelled behaviour. `module->mem` holds the file bytes (read only).
bool tfol_module_prove(const TfolModule* module);

// Reads inside the module; outside it they read 0 and writes are dropped.
uint8_t tfol_module_u8(const TfolModule* module, uint32_t offset);
uint32_t tfol_module_u32(const TfolModule* module, uint32_t offset);
void tfol_module_poke(TfolModule* module, uint32_t offset, uint8_t value);

// Turns an original address into a file offset.
uint32_t tfol_module_offset(const TfolModule* module, uint32_t address);

// Decodes the event at `at`; `note_has_length`: the voice has a note length, so a note is one byte.
TfolEvent tfol_module_read_event(const TfolModule* module, uint32_t at, bool note_has_length);

TfolSong tfol_module_song(const TfolModule* module, uint32_t song);
TfolInstrument tfol_module_instrument(const TfolModule* module, uint32_t instrument_table, uint8_t index);
