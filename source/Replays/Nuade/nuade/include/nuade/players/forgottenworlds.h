// SPDX-License-Identifier: MIT
// Forgotten Worlds player (the sound driver of the Amiga game; "fw." / ".fw" modules, magic "FWMP").

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VOICES 4

// The CIA clock: the timebase of FW_TICK_LENGTH.
#define FW_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / FW_TIMEBASE_HZ seconds.
#define FW_TICK_LENGTH 14187

typedef struct FwPlayer FwPlayer;

typedef struct FwTickResult {
    bool looped; // every voice has run its loop command or its end command since the last report
    bool ended;  // every voice has run its end command
} FwTickResult;

typedef struct FwVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // instrument number of the last instrument command, -1 if none
} FwVoiceState;

typedef enum FwStage {
    FwStage_Attack,
    FwStage_Decay,
    FwStage_Hold,
    FwStage_Release,
    FwStage_Done,
} FwStage;

typedef struct FwModule {
    uint8_t* data; // the file (nuade_paula_padded_copy: a sample plays zeros past its end); the end command changes it
    uint8_t* original; // the file as loaded; `data` is restored from it at the start
    size_t size;       // without the padding
    uint32_t patterns;
    uint32_t instruments;
    uint32_t samples;
} FwModule;

// An instrument record: six bytes (the rest of the eight is not read).
typedef struct FwInstrument {
    uint8_t sample;  // offset of the sample record in the sample table
    uint8_t sustain; // level the decay stops at
    uint8_t attack;  // the level a note starts at, and the step of the attack
    uint8_t decay;   // step of the decay (added, so negative values lower the level)
    uint8_t hold;    // ticks the sustain level is held; 0: no envelope, the level stays
    uint8_t release; // step of the release
} FwInstrument;

// A sample record: four words.
typedef struct FwSample {
    uint32_t start; // offset of the first block
    uint16_t words;
    uint32_t loop; // offset of the block the sample repeats
    uint16_t loop_words;
} FwSample;

typedef struct FwVoice {
    uint32_t loop_start; // the start of the stream; the loop command jumps here
    uint32_t stream;     // where the next command is read when the length has run out; set by notes and rests only
    bool has_return;     // a call command has run
    uint32_t return_to;  // set by the call command
    uint8_t ticks;       // ticks until the next command
    bool has_instrument; // an instrument command has run
    int instrument_number;
    FwInstrument instrument;
    FwSample sample;
    uint16_t period;
    uint8_t level; // envelope level, 0..0x40 (the start level can be more)
    FwStage stage;
    uint8_t hold_left;
    bool wrapped; // ran the loop or end command since the last loop report
    bool ended;   // ran the end command
    bool stalled; // the stream is broken: the voice reads no more commands
    int shown_note;
    NuadePaulaChannel paula;
} FwVoice;

struct FwPlayer {
    FwModule module;
    FwVoice voices[FW_VOICES];
    // The end command clears the first word of the repeat block that the last instrument command of the tick (of any
    // voice) picked. Without such a command in the tick it clears a word of the caller's memory; that is not modelled.
    bool end_target_known;
    uint32_t end_target; // offset of the repeat block
};

// The eagleplayer's Check2 (the "FWMP" magic), plus the size the header and the song table need.
bool fw_probe(const uint8_t* data, size_t size);

// 0 when the module is not one the eagleplayer plays.
FwPlayer* fw_create(const uint8_t* data, size_t size);
void fw_destroy(FwPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
void fw_start(FwPlayer* player, NuadeVoices* voices);

FwTickResult fw_tick(FwPlayer* player, NuadeVoices* voices);

FwVoiceState fw_voice_state(const FwPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
FwPlayer* fw_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
