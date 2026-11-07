// SPDX-License-Identifier: MIT
// Mike Davies: the module (a memory image of the game's driver), the replayer versions and the typed readers of the
// records of its tables and tracks. The pointers in the data are image addresses that the eagleplayer's relocation
// turns into load addresses (MD_LOAD_ADDRESS + offset in the module), so after md_module_parse they are load
// addresses; the readers take them and md_offset turns one into an offset.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/mikedavies.h>

#define MD_INSTRUMENT_BYTES 0x18u
#define MD_TRACK_POINTER_BYTES 4u
// A subsong: the track pointers of its voices.
#define MD_SUBSONG_BYTES (MD_VOICES * MD_TRACK_POINTER_BYTES)

// The address the eagleplayer's module lies at in UADE's memory. The relocation turns the pointers of the data into
// addresses from here, and a read past a table sees them as the code would.
#define MD_LOAD_ADDRESS 0xF000u

// A track pointer of a voice that does not play.
#define MD_NO_TRACK UINT32_MAX

// The track pointers of a subsong, one per voice (load addresses; every pointer below is one).
typedef struct MdSubsong {
    uint32_t track[MD_VOICES];
} MdSubsong;

// The record of an instrument: the sample (module offset), its length, the period and volume it starts with, the
// transposition it adds to the notes and the repeat offset in bytes.
typedef struct MdInstrument {
    uint32_t sample; // load address
    uint16_t length_words;
    uint16_t period;
    uint16_t volume;
    int16_t transpose;
    int16_t loop_offset;
} MdInstrument;

// The step of a track: a note and its duration.
typedef struct MdNote {
    uint16_t note;
    uint16_t duration;
} MdNote;

// The record of the call command: play another track `repeats` times, transposed.
typedef struct MdCall {
    uint16_t repeats;
    int16_t transpose;
    uint32_t track;
} MdCall;

// The entries of a track from this value on are commands, below it notes. A command is at an offset from the version's
// command_end, except the two that end the song.
#define MD_FIRST_COMMAND 0x80u
#define MD_COMMAND_CALL_OFFSET 1u
#define MD_COMMAND_LOOP_NEXT_OFFSET 9u
#define MD_COMMAND_LOOP_START_OFFSET 10u
#define MD_COMMAND_RESTART 0xFFu
#define MD_COMMAND_STOP 0xFEu

// What a track entry means.
typedef enum MdCommand {
    MdCommand_Note,       // an entry below MD_FIRST_COMMAND: a note, followed by its duration
    MdCommand_End,        // the track starts again; a call counts down and ends with it
    MdCommand_Call,       // play another track
    MdCommand_LoopNext,   // go back to the loop start until the loop count runs out
    MdCommand_LoopStart,  // loop start with a count
    MdCommand_Restart,    // the song starts again
    MdCommand_Stop,       // all voices stop
    MdCommand_Instrument, // any other command: the instrument number
} MdCommand;

// The eagleplayer's Check2 and everything InitPlayer does to the module. False when the module is refused or the
// code is unknown. `data` is a writable chip memory copy (nuade_paula_padded_copy) of `size` bytes.
bool md_module_parse(MdModule* module, uint8_t* data, uint32_t size);

// The offset in the module of a load address; false for one outside it.
bool md_offset(const MdModule* module, uint32_t address, uint32_t* out);

bool md_read_subsong(const MdModule* module, uint32_t index, MdSubsong* out);
bool md_read_instrument(const MdModule* module, uint32_t index, MdInstrument* out);

// What the entry `code` of a track means in this version.
MdCommand md_command(const MdModule* module, uint16_t code);

// The code at `address` in a track: a note and its duration, or a command (and the instrument number of an instrument
// command).
bool md_read_code(const MdModule* module, uint32_t address, uint16_t* out);
bool md_read_note(const MdModule* module, uint32_t address, MdNote* out);
bool md_read_call(const MdModule* module, uint32_t address, MdCall* out);
// The count of a loop start: a word, or in the byte tracks the byte after the command.
bool md_read_loop_count(const MdModule* module, uint32_t address, uint16_t* out);
// The size of a track entry in bytes: 2 for words, 1 for bytes. A note takes two entries, an instrument or the end
// of a loop one, a loop start two, a call five.
uint16_t md_unit(const MdModule* module);

// The word of the period table `byte_offset` bytes from its start (negative: the bytes before it).
bool md_read_period(const MdModule* module, int16_t byte_offset, uint16_t* out);
