// SPDX-License-Identifier: MIT
// Medley: the module layout, shared by the module reader and the replayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/players/medley.h>

// The most items (commands, and pairs with a duration of 0) one track step may run. A track that loops without reaching
// a note hangs the original; the port ends the song instead.
#define MDL_MAX_STEP_COMMANDS 4096
#define MDL_NOTES_PER_OCTAVE 12

// An offset that points nowhere (an empty table entry).
#define MDL_NO_OFFSET 0xffffffffu

uint8_t mdl_u8(MdlModule* module, uint32_t offset);
uint16_t mdl_u16(MdlModule* module, uint32_t offset);
uint32_t mdl_u32(MdlModule* module, uint32_t offset);

// The eagleplayer's Check2 and the table setup of its InitPlayer: false unless the file is large enough for the four
// tables it names. Fills everything of `module` but `bytes` and `unmapped`.
bool mdl_module_open(MdlModule* module, const uint8_t* data, size_t size);

// What table entry `index` of a table points to (the entry address plus the entry's long), MDL_NO_OFFSET for an empty
// entry. The index is not checked against the table size: the replayer reads whatever lies behind a table.
uint32_t mdl_table_target(MdlModule* module, MdlTable table, uint8_t index);

// Song `number` (1 up to the subsong count); false for an empty entry or a header outside the file.
bool mdl_song(MdlModule* module, uint8_t number, MdlSong* out);
// The instrument voice `voice` starts with: its own, else the song's default, else instrument 1.
uint8_t mdl_song_voice_instrument(const MdlSong* song, int voice);
// The first byte of track `number` (behind its header), MDL_NO_OFFSET for an empty entry.
uint32_t mdl_track_start(MdlModule* module, uint8_t number);
// The same entry read as command 4 does: the entry's long is added to the start of the track table, not to the entry,
// and the header is not skipped. MDL_NO_OFFSET for an empty entry.
uint32_t mdl_track_from_table_start(MdlModule* module, uint8_t track);

// A list of tracks to pick from: a random voice finds it at the start of its own track (the count at byte 1, the pairs
// from byte 2), command 4 behind the command (the pairs only). A pair is a byte and a track number.
#define MDL_RANDOM_LIST_PAIRS 2u
uint8_t mdl_random_list_count(MdlModule* module, uint32_t track_start);
uint8_t mdl_choice_track(MdlModule* module, uint32_t first_pair, uint16_t choice);

typedef enum MdlModulatorKind {
    MdlModulatorKind_Off = 0,
    MdlModulatorKind_Vibrato = 1, // the period, around the middle of the modulator range
    MdlModulatorKind_Tremolo = 2, // lowers the volume
    MdlModulatorKind_Window = 3,  // moves the window of a looped sample
    MdlModulatorKind_Phase = 4,   // moves the first read of a synth wave
    MdlModulatorKind_PeriodUp = 5,
    MdlModulatorKind_PeriodDown = 6,
    MdlModulatorKind_Tone = 7, // from this kind up: the tone offset of a synth wave
} MdlModulatorKind;

// A modulator can start at the top or at half level when a voice flag is set; the flags are numbered 1 and 2, any other
// number reads further into the voice record.
#define MDL_VOICE_FLAG_COUNT 2
bool mdl_voice_flag_valid(uint8_t flag);

// The instrument record at `offset` of the module.
MdlInstrument mdl_instrument(MdlModule* module, uint32_t offset);
// The offset of instrument `number`'s record, MDL_NO_OFFSET for an empty entry.
uint32_t mdl_instrument_offset(MdlModule* module, uint8_t number);

// Wave `number`; false for an empty entry.
bool mdl_wave(MdlModule* module, uint8_t number, MdlWave* out);

// A sampled instrument plays the wave of the note's range, the nearest below that has one.
uint8_t mdl_note_region(uint8_t note);
uint8_t mdl_region_wave(const MdlInstrument* instrument, uint8_t region);
// The period table index of a note on a sampled instrument, with the wave it plays.
uint8_t mdl_sample_period_index(uint8_t note, const MdlInstrument* instrument, const MdlWave* wave);

// Where a looped instrument's window starts in the wave data (bytes from the file start): the second half is `length`
// further on (the replayer adds it as a signed word), `position` is the window position within a half. Negative when
// that points before the file.
int64_t mdl_window_offset(const MdlWave* wave, bool second_half, uint16_t position);
// True when `words` words (0 = 65536) from `offset` lie inside the file.
bool mdl_sample_inside(const MdlModule* module, int64_t offset, uint16_t words);

// A track byte with the top bit set is a command (its low nibble) with a parameter byte; any other byte is a note.
#define MDL_COMMAND_BIT 0x80
#define MDL_COMMAND_MASK 0x0f

typedef enum MdlCommand {
    MdlCommand_End = 0,
    MdlCommand_Nop = 1,
    MdlCommand_Volume = 2,
    MdlCommand_Instrument = 3,
    MdlCommand_RandomTrack = 4,
    MdlCommand_Call = 5,
    MdlCommand_LoopStart = 6,
    MdlCommand_LoopEnd = 7,
    MdlCommand_Transpose = 8,
} MdlCommand;

// The replayer's own tables.
#define MDL_PERIOD_COUNT 110
#define MDL_ARPEGGIO_COUNT 6
#define MDL_ARPEGGIO_FRAMES 4

typedef struct MdlArpeggio {
    uint8_t frames;
    int8_t step[MDL_ARPEGGIO_FRAMES][MDL_VOICES];
} MdlArpeggio;

// A period index past the table reads the words behind it: the replayer's other tables, then the voice records. The
// songs reach two words of voice 0's record: the call depth in bytes, and the eight return addresses of the call stack
// (UADE loads the module at MDL_MODULE_ADDRESS), the high word first. The indexes are the distance in words from the
// table start to the record field (hunk offsets 0xd0a and 0xe4e + 0xaa / + 0xae).
#define MDL_PERIOD_CALL_DEPTH_INDEX 247u
#define MDL_PERIOD_CALL_STACK_INDEX 249u
#define MDL_PERIOD_CALL_STACK_WORDS (MDL_STACK_DEPTH * 2u)
#define MDL_MODULE_ADDRESS 0x11000u

// True for a period index that reads the call stack of voice 0.
bool mdl_period_is_call_stack_word(uint32_t index);
// True for a period index the port models: in the table or one of the voice 0 words.
bool mdl_period_index_modelled(uint32_t index);

// True when every read the replayer can make while playing any subsong lies inside the file or in the modelled
// memory, whatever the timing (see mdl_coverage.c). Sets module->unmapped for the first thing it cannot prove.
bool mdl_module_covered(MdlModule* module);

extern const uint16_t mdl_period_table[MDL_PERIOD_COUNT];
extern const MdlArpeggio mdl_arpeggio_table[MDL_ARPEGGIO_COUNT];
