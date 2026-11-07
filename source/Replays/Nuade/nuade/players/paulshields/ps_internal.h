// SPDX-License-Identifier: MIT
// Paul Shields player: types shared by the module reader (psh_module.c, psh_check.c) and the replayer (paulshields.c,
// psh_voice.c, psh_sequencer.c, psh_old.c).
//
// A module starts with a table of 16 sample records (10 bytes each in the short layout, 32 bytes in the other two),
// followed by the song header: the pitch and volume envelope tables, or the instrument table of the oldest layout, and
// for every voice a start volume and the offset of its order list. The order lists, patterns and envelopes follow, the
// sample bytes come last.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/paulshields.h>

#define PSH_ENVELOPE_END 0x80    // the envelope stays at its last value
#define PSH_WAVE_DATA_SIZE 460   // bytes of the replayer's data hunk
#define PSH_SAMPLE_ENTRY_SIZE 16 // bytes of an entry of the replayer's own sample table

// Pattern and order list entries one voice reads in one tick.
#define PSH_COMMAND_LIMIT 1024

// The order list words from 0xFFE8 up are codes, in steps of 4; below, bit 15 is the pattern's repeat flag and the
// rest its offset.
#define PSH_ORDER_CODE_MIN 0xFFE8u
#define PSH_ORDER_REPEATS_BIT 0x8000u
#define PSH_ORDER_OFFSET_MASK 0x7FFFu

// The pattern bytes of the short and long layouts. The replayer jumps through a table of 4-byte entries, so a command
// is a multiple of 4; the entries past the table are the code of the order list handlers, which makes 0x2C, 0x30 and
// 0x3C aliases of order list codes. 0x24, 0x28, 0x34 and 0x38 run an order list handler on whatever the replayer left
// in its address register: undefined, and refused when the song uses them.
typedef enum PshPatternCommand {
    PshPatternCommand_Note = 0x00,
    PshPatternCommand_Sample = 0x04,
    PshPatternCommand_VolumeOverride = 0x08,
    PshPatternCommand_NextPattern = 0x0C,
    PshPatternCommand_Rest = 0x10,
    PshPatternCommand_DirectPitchEnvelope = 0x14,
    PshPatternCommand_VolumeEnvelope = 0x18,
    PshPatternCommand_NotePitchEnvelope = 0x1C,
    PshPatternCommand_Slide = 0x20,
    PshPatternCommand_RestartOrderList = 0x2C,
    PshPatternCommand_Stop = 0x30,
    PshPatternCommand_NextPatternAlias = 0x3C,
} PshPatternCommand;

// The pattern bytes of the old layout: below 0x80 a note, from 0x80 up commands. The filter commands switch Paula's
// low-pass filter, which UADE keeps off; the player has no filter to switch. The aliases are entries into the code of
// other commands.
typedef enum PshOldPatternCommand {
    PshOldPatternCommand_Rest = 0x80,
    PshOldPatternCommand_Slide = 0x84,
    PshOldPatternCommand_VolumeOverride = 0x88,
    PshOldPatternCommand_Instrument = 0x8C,
    PshOldPatternCommand_NextPattern = 0x90,
    PshOldPatternCommand_FilterOn = 0x94,
    PshOldPatternCommand_FilterOff = 0x98,
    PshOldPatternCommand_FilterOffAlias = 0x9C,
    PshOldPatternCommand_NoOperation = 0xA4,
    PshOldPatternCommand_VolumeOverrideAlias = 0xB0,
    PshOldPatternCommand_LevelOnly = 0xB4, // sets the level but not the volume override
} PshOldPatternCommand;

const PshLayout* psh_layout(PshKind kind);

// The module as the player keeps it. `chip` is one allocation from nuade_paula_padded_copy: the module bytes, the two
// bytes the voices' idle streams read, the replayer's own data hunk, and the zeros after them. Offsets name bytes of
// it; a read of the module returns 0 outside the module and the idle stream bytes.

// Reads the layout, checks it like Check2 and InitPlayer and builds the sample table. False when the eagleplayer would
// refuse the file, or when the song uses a pattern command, order code or sample number that has no defined meaning.
bool psh_module_load(PshModule* m, const uint8_t* data, size_t size);
void psh_module_free(PshModule* m);

uint8_t psh_u8(const PshModule* m, int64_t offset);
uint16_t psh_u16(const PshModule* m, int64_t offset);

// The sample table entry of a sample command's number; null for a number the table does not hold.
const PshSample* psh_sample_number(const PshModule* m, unsigned number);

// The sample table entry an instrument record of the old layout names; null when it names none.
const PshSample* psh_instrument_sample(const PshModule* m, int64_t record);

// The AUDxLC pointer of a chip offset: bit 0 is ignored by Paula; an offset outside the chip memory plays silence.
const int8_t* psh_chip_pointer(const PshModule* m, int64_t offset);

// The action of an order list word from PSH_ORDER_CODE_MIN up; false for a word that is no code.
bool psh_order_action(const PshLayout* layout, uint16_t word, PshOrderAction* action);

// The static walk of the song (psh_check.c): false when a voice reaches something the replayer does not define.
bool psh_check_song(const PshModule* m);

// The replayer's state

// One tick of one voice (psh_sequencer.c for PshKind_Short and PshKind_Long, psh_old.c for PshKind_Old).
void psh_step_voice(PshPlayer* p, uint32_t channel);
void psh_step_voice_old(PshPlayer* p, uint32_t channel);

// What the two sequencers do alike (psh_voice.c)

void psh_volume_slide(PshVoice* v);
void psh_volume_envelope(const PshModule* m, PshVoice* v);
void psh_pitch_envelope(const PshModule* m, PshVoice* v);
void psh_release_level(PshVoice* v);
void psh_add_period_slide(PshVoice* v);

// An envelope is selected by the offset of its first byte, the number of ticks per step.
void psh_select_volume_envelope(const PshModule* m, PshVoice* v, int64_t offset);
void psh_select_pitch_envelope(const PshModule* m, PshVoice* v, int64_t offset);
void psh_select_sample(PshVoice* v, const PshSample* sample);

// Reads a pattern duration at the voice's stream position.
void psh_read_duration(const PshModule* m, PshVoice* v);

// The period of `note_units` (a note byte * 8) with the voice's tunings. The oldest replayer returns the longest
// period for a negative sum (`negative_is_longest`).
uint16_t psh_note_period(const PshVoice* v, uint16_t note_units, bool negative_is_longest);

// The period runs from `start` to `target` in `duration` ticks.
void psh_start_slide(PshVoice* v, uint16_t start, uint16_t target, uint16_t duration);

// The note starts: the channel's DMA goes off and gets the sample, the envelopes restart.
void psh_start_note(PshPlayer* p, uint32_t channel);

// Writes the period and the volume registers of a channel.
void psh_write_registers(PshPlayer* p, uint32_t channel, uint16_t period);

// The note byte as semitones from C-0, for the display.
int psh_display_note(uint8_t note);

void psh_stop_voice(PshVoice* v);

// The pattern command "next pattern": false when the voice stopped.
bool psh_next_pattern(PshPlayer* p, uint32_t channel);

// The order list code "loop" run as a pattern command: the list starts over and its next entry is read.
bool psh_restart_order_list(PshPlayer* p, uint32_t channel);

// The order list code "stop" run as a pattern command: the voice stops and the song ends. Always false.
bool psh_stop_song(PshPlayer* p, uint32_t channel);
