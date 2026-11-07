// SPDX-License-Identifier: MIT
// Rob Hubbard ST module: the eagleplayer's checks, the replayer build the module carries, the byte encoding of the
// sequence data and the module memory the replayer works on.

#pragma once

#include <nuade/players/robhubbardst.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The stream bytes. 0x00..0x7F is a note; 0x80..0xB7 is a command of the jump table (low six bits), then the ranges
// below.
#define RHST_NOTE_END 0x80u
#define RHST_COMMAND_MASK 0x3Fu // the jump table index of a command byte
#define RHST_COMMAND_LAST 0xB7u
#define RHST_TEMPO_FIRST 0xB8u      // tempo 1..8
#define RHST_INSTRUMENT_FIRST 0xC0u // selects one of RHST_INSTRUMENT_COUNT instruments
#define RHST_ENVELOPE_FIRST 0xD0u   // volume envelope: start volume, then three rate bytes
#define RHST_LENGTH_FIRST 0xE0u     // length of the following notes, 1..32 steps
#define RHST_INSTRUMENT_COUNT 16u
#define RHST_STEP_LIMIT 4096u // stream bytes one sequencer step reads at most; the module walk proves it
#define RHST_LIST_END 0x87u   // ends an instrument's semitone list (the same byte as the next-track command)

typedef enum RhstCommand {
    RhstCommand_Rest,
    RhstCommand_VibratoOff,
    RhstCommand_VibratoFull,
    RhstCommand_VibratoHalf,
    RhstCommand_Slide,
    RhstCommand_PortamentoDown,
    RhstCommand_PortamentoUp,
    RhstCommand_NextTrack,
    RhstCommand_Vibrato,
    RhstCommand_NoiseFreq,
    RhstCommand_ToneOnly,
    RhstCommand_NoiseOnly,
    RhstCommand_ToneAndNoise,
    RhstCommand_Percussion,
    RhstCommand_Stop,
    RhstCommand_RestKeep,
    RhstCommand_Tie,
    RhstCommand_NoiseTrigger,
    RhstCommand_Count, // entries of the jump table
} RhstCommand;

// null when the eagleplayer would not play the module or the port does not model it.
RhstModule* rhst_module_open(const uint8_t* data, size_t size);
void rhst_module_close(RhstModule* module);

// Reads of the module image; 0 outside the module.
uint8_t rhst_module_u8(const RhstModule* module, uint32_t offset);
uint16_t rhst_module_u16(const RhstModule* module, uint32_t offset);
uint32_t rhst_module_u32(const RhstModule* module, uint32_t offset);

// The song the init starts for a subsong.
uint32_t rhst_module_played_song(const RhstModule* module, uint32_t subsong);

// Offset of the track list of a voice of a song.
uint32_t rhst_module_track_list(const RhstModule* module, uint32_t song, uint32_t voice);

// The track list whose wrapping calls SongEnd: the longest list of the subsong, the last of equally long ones.
uint32_t rhst_module_longest_list(const RhstModule* module, uint32_t subsong);

// Offset of the semitone list an instrument selects.
uint32_t rhst_module_instrument(const RhstModule* module, uint32_t index);

// Write the voice into its record in the image (a no-op for builds whose samples do not reach the records).
void rhst_module_store_voice(RhstModule* module, uint32_t index, const RhstVoice* voice);
