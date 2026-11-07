// SPDX-License-Identifier: MIT
// Riff Raff: the module (an Amiga executable that carries its replayer), the known replayer versions and the replayer's
// state.
//
// Module layout (offsets in the code hunk; the replayer's tables and the song data are addressed by absolute pointers
// that the hunk's relocation table turns into offsets):
//   +0   moveq #-1,d0 / rts, "RIFF", "RAFF"
//   +12  u32 second pass entry, u32 init entry, u32 subsong count, u32 tick entry (0: none), u32 name, author, info
//        pointers, u32 sample pointer table, u32 sample length table, ... (see rr_module.c)
//   the replayer's code (from the init entry to the DMA bit table), then the DMA bit table (1, 2, 4, 8), the period
//   table, the rate table, the replayer's variables, the voice records, the sample, track and pattern tables, the song.
// Voice record (one per voice, 0x36 or 0x3c bytes) and variables are written by the replayer and are not module data,
// except the instrument and transposition words, which the Looping / Clamped init routines leave as the module has
// them.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_paula_channel.h"
#include <nuade/players/riffraff.h>

#define RR_MAX_MODULE_BYTES (1u << 20)
#define RR_MAX_SUBSONGS 16
#define RR_SUBSONG_TABLE_BYTES 18 // four track list pointers and the voice mask word
#define RR_SUBSONG_MASK_OFFSET 16 // ... the mask word behind the pointers
#define RR_PERIOD_ENTRY_BYTES 2u
#define RR_RATE_ENTRY_BYTES 2u
#define RR_PATTERN_SLOTS 256u           // pattern numbers are a byte
#define RR_VOICE_INSTRUMENT_OFFSET 0x30 // words of a voice record that the init routine does not write
#define RR_VOICE_TRANSPOSE_OFFSET 0x34
// The bytes of the song data.
#define RR_TRACK_END 0xFFu // the end of a track list (OneShot: any byte with bit 7)
#define RR_TRACK_TRANSPOSE 0xFEu
#define RR_PATTERN_END 0xFFu
#define RR_COMMAND_BASE 0x80u // a pattern byte from here on is a command (the byte minus this indexes the jump table)
// Iterations of the event loop (a pattern byte or a track list byte) one tick may run before the port stops a voice;
// the open-time walk refuses a module that needs more.
#define RR_EVENT_BUDGET (1u << 16)
#define RR_ZERO_BUFFER_WORDS 2 // the eagleplayer's four zero bytes of chip memory, kept at offset `size`

// Pattern commands (the byte minus 0x80 indexes the version's jump table).

typedef enum RrCommand {
    RrCommand_SetSample,
    RrCommand_VolumeSlide,
    RrCommand_MasterFade,
    RrCommand_SetVolume,
    RrCommand_Modulation, // sets a flag that only drives ADKCON writes that clear bits: no effect
    RrCommand_Loop,       // Bomb Fusion: sets fields nothing reads
    RrCommand_Rest,
    RrCommand_RestSilence,
    RrCommand_PitchSlide,
    RrCommand_PitchSlideOff,
    RrCommand_Portamento, // reads a register the caller leaves undefined: refused
    RrCommand_Invalid,
} RrCommand;

typedef struct RrCommandInfo {
    RrCommand command;
    uint8_t arg_bytes;
} RrCommandInfo;

RrCommandInfo rr_command_info(RrDriver driver, uint8_t byte);

// The module readers. Each returns false when the bytes are outside the module, inside the replayer's code or
// variables, or a relocated long (the original would read other values there).

bool rr_module_load(RrModule* module, const uint8_t* data, size_t size);
void rr_module_free(RrModule* module);

bool rr_module_u8(const RrModule* module, uint32_t at, uint8_t* out);
bool rr_module_u16(const RrModule* module, uint32_t at, uint16_t* out);
// A pointer table entry: a relocated long, or a zero the eagleplayer replaced by the module start. `out` is an offset.
bool rr_module_pointer(const RrModule* module, uint32_t at, uint32_t* out);
// The bytes of a sample (a length of 0 words plays 65536) can be played.
bool rr_module_sample_ok(const RrModule* module, uint32_t at, uint16_t words);

// One sample table entry: AUDxLC (bit 0 dropped as Paula does) and the length in words.
typedef struct RrSample {
    uint32_t start;
    uint16_t words;
} RrSample;

// The song's start state: the track list of each voice and which voices play.
typedef struct RrSongStart {
    uint32_t track_list[RR_VOICES];
    bool enabled[RR_VOICES];
} RrSongStart;

// What a voice record holds before the first tick: the replayer's init routine does not write the instrument and the
// transposition words (Looping, Clamped), so they are the module's own bytes.
typedef struct RrVoiceStart {
    uint16_t instrument;
    uint16_t transpose;
} RrVoiceStart;

bool rr_module_song_start(const RrModule* module, uint32_t subsong, RrSongStart* out);
bool rr_module_voice_start(const RrModule* module, uint32_t voice, RrVoiceStart* out);
// The silent word the replayer points its repeat at (the eagleplayer's zero bytes for the versions with a loop table).
uint32_t rr_module_silence(const RrModule* module);
bool rr_module_track_list_ends(const RrModule* module, uint8_t byte);
bool rr_module_period(const RrModule* module, uint32_t index, uint16_t* out);
bool rr_module_rate(const RrModule* module, uint32_t index, uint16_t* out);
bool rr_module_sample(const RrModule* module, uint32_t instrument, RrSample* out);
bool rr_module_loop(const RrModule* module, uint32_t instrument, RrSample* out);
bool rr_module_pattern(const RrModule* module, uint32_t number, uint32_t* out);

// Plays every voice of every subsong through the track lists and patterns without sound: all they read must be
// reproducible, and no cycle of a track list may be without a note or a rest (the replayer would never return).
bool rr_module_validate(const RrModule* module);
