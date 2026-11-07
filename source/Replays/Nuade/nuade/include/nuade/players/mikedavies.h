// SPDX-License-Identifier: MIT
// Mike Davies' sound driver (Amiga games: Strider, Mercs, Alien Storm, World Cup USA 94, ...), translated from the UADE
// eagleplayer "MikeDavies". Every module is a memory image of the game's driver: the replayer's code, its variables,
// the tables and the song. The eagleplayer finds the tables by the instructions of the code, relocates the
// pointers of the data and calls the init and tick routines of the module.

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

#define MD_VOICES 4

// The CIA clock: the timebase of MD_TICK_LENGTH.
#define MD_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MD_TIMEBASE_HZ seconds.
#define MD_TICK_LENGTH 14187

#define MD_MAX_OPERANDS 128u

typedef struct MdPlayer MdPlayer;

typedef struct MdTickResult {
    bool looped; // the eagleplayer's patches call SongEnd: a track restarts the song, or a loop goes back before a stop
    bool ended;  // a track stopped the replayer, or the song left the data the port models
} MdTickResult;

typedef struct MdVoiceState {
    int period;     // the last period written, 0 if none
    int instrument; // the instrument number last selected, -1 if none
} MdVoiceState;

// The replayer versions differ in a few places; the code of a version is known by hash.
typedef struct MdLayout {
    uint64_t code_hash;   // FNV-1a over the code, the pointers into the tables and variables left out, see md_module.c
    uint32_t code_end;    // module offset of the end of the code, which starts at the init routine
    uint16_t command_end; // the track command that ends a track; the others are at MD_COMMAND_*_OFFSET from it
    uint32_t periods;     // module offset of the period table
    uint16_t min_period;  // a period below this is raised to it, or silences the voice, see floor_clamps
    uint8_t passes;       // sequencer and hardware passes per tick
    bool byte_tracks;     // the tracks are bytes (note, duration), otherwise words
    bool rest_note;       // the note 0 keeps both transposes out of the period and the voice's DMA off
    bool floor_clamps;    // a period below min_period is raised to it; otherwise the voice is dropped from its pass
    bool interpolates;    // the period is interpolated between two table entries by the instrument's fraction
    bool loop_back_ends_song; // the eagleplayer calls SongEnd when a loop goes back and the stop command follows it
} MdLayout;

// The addresses of the operands the eagleplayer relocates in the code (and in what follows it up to the period table).
typedef struct MdOperands {
    uint32_t at[MD_MAX_OPERANDS];
    uint32_t count;
} MdOperands;

typedef struct MdModule {
    uint8_t* data; // the module in chip memory, relocated; writable
    uint32_t size;
    const MdLayout* layout;
    MdOperands operands; // the code holds the module's load address there, which the port does not model
    uint32_t subsong_count;
    uint32_t subsongs;          // table of 4 track pointers each
    uint32_t instruments;       // table of MD_INSTRUMENT_BYTES records
    uint32_t instruments_limit; // the replayer takes an instrument number at or past this as instrument 0
    uint32_t periods;           // table of words
} MdModule;

// A track and where a voice is in it. The voice has the main track and, after a call command, a second one.
typedef struct MdTrack {
    uint32_t pointer; // load address; MD_NO_TRACK: the voice is off. The second track is off at 0
    uint16_t position;
    uint16_t loop_count;
    uint16_t loop_position;
    int16_t transpose;
} MdTrack;

typedef struct MdVoice {
    NuadePaulaChannel paula;
    uint16_t period;
    uint16_t volume;
    bool trigger;    // a note or an instrument is waiting for the hardware pass
    uint32_t sample; // load address
    uint16_t length_words;
    int16_t loop_offset;
    uint16_t duration; // ticks left in the note
    uint16_t repeats;  // plays of the called track left; the end of a track counts it down
    MdTrack main;
    MdTrack called;
    uint16_t command;             // the last instrument command
    int16_t instrument_transpose; // added to the notes
    uint16_t note;                // the last note with its transposes; 0 keeps the DMA off in the later hardware pass
} MdVoice;

struct MdPlayer {
    uint8_t* chip; // the module, padded for the Paula channels
    MdModule module;
    MdVoice voices[MD_VOICES];
    uint32_t subsong;       // 0-based
    uint64_t commands_left; // track entries the sequencer may still run through; the dry run limits them
    bool enabled;
    bool fault;  // the module led out of what the port models
    bool looped; // set within a tick
    bool ended;
};

// Null when the module is not one the port plays: the eagleplayer's Check2, a replayer version known by its code, and
// songs that stay inside the module.
MdPlayer* md_create(const uint8_t* data, size_t size);
void md_destroy(MdPlayer* player);

uint32_t md_subsong_count(const MdPlayer* player);

// (Re)start a subsong (0-based): the eagleplayer's InitSound.
bool md_start(MdPlayer* player, uint32_t subsong, NuadeVoices* voices);

MdTickResult md_tick(MdPlayer* player, NuadeVoices* voices);

MdVoiceState md_voice_state(const MdPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
MdPlayer* md_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
