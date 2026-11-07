// SPDX-License-Identifier: MIT
// Maximum Effect: the song file (MAX.), the sample file (SMP.) and their typed readers.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/players/maximumeffect.h>

// The period table: three octaves (ProTracker periods 856 .. 113 and the notes above the table's last octave).
#define MXE_NOTE_COUNT 36
// Notes, portamento targets and arpeggio notes are byte offsets into the period table.
#define MXE_PERIOD_TABLE_BYTES (MXE_NOTE_COUNT * 2)
#define MXE_LIST_ENTRY_BYTES 6
// A position list is addressed with a signed 16-bit offset.
#define MXE_MAX_LIST_BYTES 0x8000u

// A subsong record: a flags word, then one position list per voice.
typedef struct MxeRecord {
    bool voice_used[MXE_VOICES]; // the record's voice n has a list
    bool one_shot;               // the voices stop at the end of their list instead of starting it again
    uint32_t list[MXE_VOICES];   // file offsets
} MxeRecord;

// One 6-byte position list entry: a pattern and the number of events to play from it, or the end mark (pattern 0)
// whose second field is the byte offset into the list to continue from.
typedef struct MxeListEntry {
    bool end;
    uint32_t pattern; // file offset
    uint16_t value;   // events (not end), loop position (end)
} MxeListEntry;

// The eagleplayer's Check2 and the header part of InitPlayer. `bytes` stays owned by the caller.
bool mxe_module_parse(MxeModule* module, const uint8_t* bytes, size_t size);

// Walks every subsong's lists and patterns once and checks everything the replayer reads from them. Sets
// `uses_instrument` and `grid_ticks`.
bool mxe_module_validate(MxeModule* module);

// Record `subsong` (below subsong_count).
MxeRecord mxe_record(const MxeModule* module, uint32_t subsong);

// The entry at `offset` into the file (the caller keeps it inside).
MxeListEntry mxe_list_entry(const MxeModule* module, uint32_t offset);

// What a pattern word is. A negative word is a note at the start of an event and the event's duration after that.
typedef enum MxeCommand {
    MxeCommand_Nop = 0,
    MxeCommand_FilterOff,
    MxeCommand_FilterOn,
    MxeCommand_SetVolume,
    MxeCommand_VibratoRestart, // two parameter bytes follow, besides the speed in the word
    MxeCommand_Vibrato,        // same bytes, the vibrato phase keeps running
    MxeCommand_VolumeSlide,    // the step is in the word
    MxeCommand_Portamento,     // two parameter bytes follow
    MxeCommand_Arpeggio,       // two parameter bytes follow
    MxeCommand_Count,
} MxeCommand;

typedef enum MxeItemKind {
    MxeItem_Note = 0,
    MxeItem_Command,
    MxeItem_Duration,
} MxeItemKind;

typedef struct MxeItem {
    MxeItemKind kind;
    // Note
    uint8_t instrument;
    uint8_t note_offset; // byte offset into the period table (even)
    bool keep_volume;    // bit 0 of the note byte
    // Command
    MxeCommand command;
    uint8_t param;    // the word's low byte
    uint8_t extra[2]; // the bytes behind the word, for the commands that have them
    // Duration
    uint16_t duration; // the word: bit 15 set, the low 15 bits count the ticks after the first
} MxeItem;

typedef enum MxeRead {
    MxeRead_Item = 0,
    MxeRead_Invalid, // runs out of the file, or a command the replayer cannot run
} MxeRead;

// Reads the item at `*pos` and advances. `first_of_event`: a negative word is a note, not a duration.
MxeRead mxe_read_item(const MxeModule* module, uint32_t* pos, bool first_of_event, MxeItem* item);

// An instrument record, 16 bytes in the sample file.
typedef struct MxeInstrument {
    uint32_t start; // sample offset in the sample file
    uint16_t length_words;
    uint16_t volume;
    uint32_t loop_start; // offset in the sample file
    uint16_t loop_words;
} MxeInstrument;

// The eagleplayer's InitPlayer part for the sample file: the instrument table must be followed by the samples, and the
// last one must end inside the file. Takes a copy of `bytes`.
bool mxe_samples_load(MxeSamples* samples, const uint8_t* bytes, size_t size);
void mxe_samples_free(MxeSamples* samples);

MxeInstrument mxe_instrument(const MxeSamples* samples, uint32_t index);

// Every instrument the song plays has its sample and its repeat inside the file.
bool mxe_samples_check(const MxeSamples* samples, const MxeModule* module);

// The period of a note: `note_offset` is a byte offset, even and inside the table.
uint16_t mxe_period(uint8_t note_offset);

// One entry of the sine table of the vibrato, indexed by a byte offset (even, below 128).
int16_t mxe_sine(uint8_t offset);

typedef enum MxeLoad {
    MxeLoad_Loaded = 0,
    MxeLoad_Missing, // no file could be read
    MxeLoad_Invalid, // a file was read and InitPlayer refuses it
} MxeLoad;

// The eagleplayer's file names for a song: the song's file name with "MAX." replaced by "SMP." (any other name: minus
// its last character), and the shared "SMP.set" when that file is missing. Stops at the first that exists (refused if
// invalid).
MxeLoad mxe_load_sample_file(const char* song_path, const NuadeIo* io, MxeSamples* samples);
