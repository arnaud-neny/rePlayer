// SPDX-License-Identifier: MIT
// Silmarils game music, translated from the UADE eagleplayer "Silmarils" (Wanted Team).
// The module format and the replayer are described in players/silmarils/.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIL_VOICES 4

// The CIA clock: the timebase of SIL_TICK_LENGTH.
#define SIL_TIMEBASE_HZ 709379
// One player tick: the eagleplayer sets the CIA timer to 0xCD0 (about 216 Hz), in 1 / SIL_TIMEBASE_HZ seconds.
#define SIL_TICK_LENGTH 0xCD0

#define SIL_USED_VOICES 3 // the music commands address three Paula channels; the fourth is never started
#define SIL_SLOTS 32
#define SIL_VARIABLES 0x24 // bytes of script variables (the original's variable area goes on with the slot table)
#define SIL_CALL_DEPTH 2   // nested script calls the original's stack area holds

typedef struct SilPlayer SilPlayer;

// The eagleplayer's module check, and then everything of the open that does not need a file next to the song. A module
// with at most two sample blocks takes its samples from a main file named by the song's file name: it is checked up to
// its script and then NeedsMain.
typedef enum SilProbe {
    SilProbe_No = 0,
    SilProbe_Yes,
    SilProbe_NeedsMain,
} SilProbe;

typedef enum SilStatus {
    SilStatus_Ok = 0,
    SilStatus_InvalidData,
    SilStatus_MissingMain, // the main file of a module without its own samples could not be read
    SilStatus_OutOfMemory,
} SilStatus;

typedef struct SilCreateResult {
    SilPlayer* player;
    SilStatus status;
} SilCreateResult;

typedef struct SilTickResult {
    bool looped; // the music reached the command where the eagleplayer calls SongEnd
    bool ended;  // the player cannot continue
} SilTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number (-1 if
// none).
typedef struct SilVoiceState {
    int note;
    int instrument;
} SilVoiceState;

typedef struct SilMemory {
    uint8_t* data;
    uint32_t size;
} SilMemory;

// What the eagleplayer's check finds in a file; offsets are from the start of the file.
typedef struct SilContainer {
    uint32_t base;          // 0, or 6 for the form with a 6 byte prefix; the offsets in the file count from here
    uint32_t header;        // base + the long at base + 0xE
    uint32_t table;         // header + 0x58: a long per block, the offset of the block from the entry itself
    uint32_t music_blocks;  // blocks with 0xFF02 as the word at +6: the subsongs
    uint32_t sample_blocks; // the other blocks
} SilContainer;

typedef struct SilImage {
    uint32_t start; // address of the file in the memory
    uint32_t size;
    SilContainer container;
} SilImage;

// The byte the eagleplayer clears in two known modules once the song is set up: the packer left no end command in
// their music, and the first byte of the block that follows makes one.
typedef struct SilPatch {
    bool active;
    uint32_t address;
} SilPatch;

typedef struct SilSlot {
    uint32_t sample;    // address of the sample data (its block + 0x10); 0 = none
    uint16_t note_base; // added to the note
} SilSlot;

typedef struct SilScript {
    SilMemory* memory;
    const SilImage* song;   // the module the script's module-relative accesses count from
    const SilImage* blocks; // the module whose blocks the sample and music operations name
    uint8_t variables[SIL_VARIABLES];
    SilSlot slots[SIL_SLOTS];
    bool slot_parameters; // some slot has a note base: the tempo is not corrected by 7
    uint32_t pc;
    uint32_t calls[SIL_CALL_DEPTH];
    uint32_t depth;
    int16_t value;        // the accumulator
    int16_t left;         // the accumulator before the operand of a binary operation was evaluated
    uint32_t* steps_left; // the statements left to run, a budget the owner shares between runs
    uint32_t nesting;
    bool failed; // set by a fetch or an access outside what the port models
    // What the script chose
    uint32_t music; // address of the music stream; 0 = none
    uint8_t tempo;
} SilScript;

// The audio interrupt handler of a channel: what it does at the next audio interrupt.
typedef enum SilHandler {
    SilHandler_None = 0,
    SilHandler_Loop,    // at the DMA start: point the repeat at the sample's loop
    SilHandler_OneShot, // at the DMA start: arm the stop
    SilHandler_Stop,    // at the end of the block: switch the channel off
} SilHandler;

typedef enum SilCommand {
    SilCommand_End = 0,    // SongEnd, then the stream starts again
    SilCommand_Note = 1,   // note, volume: stops the channel; with a volume starts the selected instrument
    SilCommand_Select = 2, // instrument slot
    SilCommand_Tail = 3,   // plays the part of the sample after its loop end, once
} SilCommand;

typedef struct SilChannel {
    uint32_t sample;    // address of the selected instrument's data; 0 = none
    uint16_t note_base; // of the selected instrument
    int instrument;
    int note;
    NuadePaulaChannel paula;
    bool interrupt_enabled; // INTENA bit of the channel
    SilHandler handler;
    uint32_t handler_sample; // the sample the handler works on
    bool want_dma;           // the tick switches the channel's DMA on once all commands ran
    bool want_interrupt;
    bool one_shot;           // the DMA run stops at the end of its block
    bool chained_tail;       // the registers hold a tail block that plays once, after the block that runs now
    uint32_t wraps_at_start; // the voice's wrap count when the current DMA run started
    bool stop_at_switch;     // the stop is armed: DMA goes off when the voice has switched to the tail
    uint32_t switch_wraps;   // the wrap count that is the switch
} SilChannel;

typedef struct SilMusic {
    SilMemory* memory;
    const SilSlot* slots;      // the script's instrument slots
    const NuadeVoices* voices; // the voices of the tick that runs (valid only during sil_music_tick); nullptr in the
                               // validation pass
    uint32_t start;            // first byte of the stream
    uint32_t position;         // next byte to read
    uint16_t rate;             // subtracted from `wait` every tick
    uint16_t wait;
    SilCommand command; // the command that bytes below 0x80 repeat; any value past the four is not modelled
    uint8_t command_channel;
    SilChannel channels[SIL_USED_VOICES];
} SilMusic;

struct SilPlayer {
    SilMemory memory;
    uint8_t* initial; // the memory as InitPlayer leaves it: every start begins from here
    SilImage song;
    SilImage main;
    bool has_main;
    SilPatch patch;
    uint16_t flag; // 1 when the script ran a second time or the samples come from the main file
    SilScript script_initial;
    SilScript script;
    SilMusic music;
    uint16_t* rates; // per subsong: the wait counter's rate per tick
    uint32_t budget; // what the script and the validation may still spend
};

SilProbe sil_probe(const uint8_t* data, size_t size);

// InitPlayer: parses the container, unpacks the samples, loads the main file, runs the module's script, and replays
// every subsong's start and the passes of its music, so that a module the port does not model is refused here.
SilCreateResult sil_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void sil_destroy(SilPlayer* player);

uint32_t sil_subsong_count(const SilPlayer* player);

// Ticks of one row of the pattern view of a subsong: the tunes play in waits of about 25 wait units, which the tempo of
// the subsong turns into ticks.
uint32_t sil_grid_ticks(const SilPlayer* player, uint32_t subsong);

// InitSound: (re)start a subsong (0-based) from its beginning; Paula channel n = voice n.
bool sil_start(SilPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
SilTickResult sil_tick(SilPlayer* player, NuadeVoices* voices);
SilVoiceState sil_voice_state(const SilPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SilPlayer* sil_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
