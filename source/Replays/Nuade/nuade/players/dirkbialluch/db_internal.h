// SPDX-License-Identifier: MIT
// Dirk Bialluch player: the two files of a song and the records the replayer reads from them.
//
// "tpu.<name>" is the song: "MSNG" (one start position per subsong, one word each), then the chunks MSEQ (the
// sequence: 8 bytes per step, a pattern number and a transpose for each voice), MPTN (patterns), MPRG (programs), MGRV
// (the groove: tick counts ended by 0xFF) and MARP (arpeggio steps ended by 0x80). MPTN and MPRG are a word n, n + 2
// word offsets and the data the offsets count from. "smp.<name>" is the chunk MSMP: a word n, n + 2 long offsets and
// the sample data the offsets count from.
//
// The replayer reads the files as memory: tables are followed by the next table, and a record that runs off its chunk
// reads the next bytes. The readers here do the same on the file bytes and read 0 past the end of the file.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/players/dirkbialluch.h>

// The largest file the readers address (offsets are 32 bit and signed in the original).
#define DB_MAX_FILE_BYTES 0x7FFFFFFFu

// Commands

// A pattern event is three bytes: the command byte and two arguments. The command byte selects the command through
// the replayer's classification table; the numbers are the table's.
typedef enum DbPatternCommand {
    DbPatternCommand_Idle = 0,            // any other byte: the voice stays on it for good
    DbPatternCommand_Note = 1,            // 0x01..0x30: note, volume, program
    DbPatternCommand_NoteWait = 2,        // 0x41..0x70: note - 0x40, wait, program (at volume 64)
    DbPatternCommand_Wait = 3,            // 0x80
    DbPatternCommand_SetGroove = 4,       // 0x81
    DbPatternCommand_FilterOn = 5,        // 0x82
    DbPatternCommand_FilterOff = 6,       // 0x83
    DbPatternCommand_Slide = 7,           // 0x90: target note, rate
    DbPatternCommand_MasterVolume = 8,    // 0xA0
    DbPatternCommand_VolumeWait = 9,      // 0xA1
    DbPatternCommand_VolumeAddWait = 10,  // 0xA2
    DbPatternCommand_VolumeSubWait = 11,  // 0xA3
    DbPatternCommand_Loop = 12,           // 0xF0
    DbPatternCommand_Signal = 13,         // 0xB0
    DbPatternCommand_SignalWait = 14,     // 0xB1
    DbPatternCommand_Arpeggio = 15,       // 0xC0
    DbPatternCommand_GotoPosition = 16,   // 0xFC
    DbPatternCommand_Stop = 17,           // 0xFD
    DbPatternCommand_NextStep = 18,       // 0xFE
    DbPatternCommand_JumpPattern = 19,    // 0xF1
    DbPatternCommand_Unused = 20,         // 0xB2: stores a byte nothing reads
    DbPatternCommand_FadeParameters = 21, // 0xA4
    DbPatternCommand_FadeToSubsong = 22,  // 0xD0
} DbPatternCommand;

// A program command is four bytes: the command byte and three arguments.
typedef enum DbProgramOp {
    DbProgramOp_Idle = 0,            // any other byte: the voice stays on it for good
    DbProgramOp_StartSample = 1,     // 0x01: period 80, sample start, DMA off
    DbProgramOp_SampleAdd = 2,       // 0x02
    DbProgramOp_SetLength = 3,       // 0x03
    DbProgramOp_SetSample = 4,       // 0x04
    DbProgramOp_SampleSub = 5,       // 0x05
    DbProgramOp_VolumeSetWait = 6,   // 0x10
    DbProgramOp_VolumeAddWait = 7,   // 0x11
    DbProgramOp_VolumeSubWait = 8,   // 0x12
    DbProgramOp_VolumeSet = 9,       // 0x20
    DbProgramOp_VolumeAdd = 10,      // 0x21
    DbProgramOp_VolumeSub = 11,      // 0x22
    DbProgramOp_BendSubWait = 12,    // 0x30
    DbProgramOp_BendAddWait = 13,    // 0x31
    DbProgramOp_BendSub = 14,        // 0x40
    DbProgramOp_BendAdd = 15,        // 0x41
    DbProgramOp_FixedNote = 16,      // 0x50
    DbProgramOp_Wait = 17,           // 0x80
    DbProgramOp_JumpOnSignal = 18,   // 0xB0
    DbProgramOp_Arpeggio = 19,       // 0xC0
    DbProgramOp_Loop = 20,           // 0xF0
    DbProgramOp_End = 21,            // 0xFF
    DbProgramOp_RequestProgram = 22, // 0xFE
} DbProgramOp;

DbPatternCommand db_pattern_command(uint8_t code);
DbProgramOp db_program_op(uint8_t code);

// The files

// Check2 and the chunk search of InitPlayer, on the song file alone. Fills the offsets of `m` that point into the song.
bool db_module_locate(DbModule* m);

// Copies the song, locates its chunks and loads the sample file next to it (ExtLoad).
DbStatus db_module_open(DbModule* m, const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void db_module_close(DbModule* m);

// Records

typedef struct DbSequenceStep {
    struct {
        uint8_t pattern;
        uint8_t transpose;
    } voice[DB_VOICES];
} DbSequenceStep;

DbSequenceStep db_sequence_step(const DbModule* m, uint16_t position);
// The first start position of a subsong (0-based) from MSNG.
uint16_t db_subsong_position(const DbModule* m, uint8_t subsong);

// Addresses (offsets into the song file) of a pattern and of a program.
uint32_t db_pattern_address(const DbModule* m, uint8_t pattern);
uint32_t db_program_address(const DbModule* m, uint8_t program);

typedef struct DbPatternEvent {
    uint8_t code;
    uint8_t first;
    uint8_t second;
} DbPatternEvent;

// The event at `position` bytes into the pattern.
DbPatternEvent db_pattern_event(const DbModule* m, uint32_t pattern, uint16_t position);

// A program is four header bytes (the first is its priority), then commands.
typedef struct DbProgramCommand {
    uint8_t code;
    uint8_t first;
    uint8_t second;
    uint8_t third;
} DbProgramCommand;

uint8_t db_program_priority(const DbModule* m, uint32_t program);
DbProgramCommand db_program_command(const DbModule* m, uint32_t program, uint16_t position);

uint8_t db_groove_step(const DbModule* m, uint32_t offset);
uint8_t db_arpeggio_step(const DbModule* m, uint8_t index);

// Samples

// An address in the model of the machine's memory: the sample file starts at DB_SAMPLE_FILE_ADDRESS, anything below is
// not sample data.
#define DB_SAMPLE_FILE_ADDRESS 0x40000u

// The start address of sample `index`: the sample data base plus its table entry.
uint32_t db_sample_address(const DbModule* m, uint8_t index);

// AUDxLC for an address: null when it is not inside the sample file. Any AUDxLEN can follow it: the file's bytes after
// the sample play as in the original, and what runs past the end of the file plays the zeros behind its padded copy.
const int8_t* db_sample_lc(const DbModule* m, uint32_t address);
