// SPDX-License-Identifier: MIT
// Howie Davies player, translated from the UADE eagleplayer "HowieDavies" (Wanted Team) and the replayers of the
// modules. A song is an Amiga executable of one code hunk: the "H.DAVIES" mark, the entry points and tables of the
// replayer, the title texts, the replayer itself with its patterns, orders and samples. The eagleplayer loads the file
// as a program and patches it, the replayer comes from the module; three versions are known (Blazing Thunder, Future
// Bike Simulator, Yogi's Great Escape).

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

#define HD_VOICES 4

// The CIA clock: the timebase of HD_TICK_LENGTH.
#define HD_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / HD_TIMEBASE_HZ seconds.
#define HD_TICK_LENGTH 14187

#define HD_MAX_HANDLERS 8

typedef struct HdPlayer HdPlayer;

typedef struct HdTickResult {
    bool looped; // every voice has reached the end of its position list: the eagleplayer calls SongEnd
    bool ended;  // no voice runs any more (or crafted data): later ticks do nothing
} HdTickResult;

typedef enum HdText {
    HdText_Title,
    HdText_Artist,
    HdText_Message,
} HdText;

typedef struct HdVoiceState {
    int period;     // the period of the last note started, 0 if none
    int instrument; // the instrument number last loaded, -1 if none
} HdVoiceState;

// The routines a command byte can lead to. A command byte b selects entry 0xFF - b of a table of code addresses; the
// pattern table is followed by the position list table, so a pattern command above the first can reach a position
// list routine. Every version owns the routines it lists, a table entry that names any other address is not modelled.
typedef enum HdHandler {
    HdHandler_Repeat,         // pattern end: play it again, or go on in the position list
    HdHandler_Instrument,     // load a sample record
    HdHandler_Speed,          // ticks per sequencer step
    HdHandler_Delay,          // rest: the next step runs after n + 1 steps
    HdHandler_MaxVolume,      // volume limit of the voice
    HdHandler_ListRestart,    // position list end: start it again
    HdHandler_ListLoopBack,   // position list: go back n bytes
    HdHandler_ListRestartOff, // position list end: start it again and switch the voice off
    HdHandler_Return,         // leave the step (the voice stays on this byte)
} HdHandler;

typedef struct HdHandlerAddress {
    uint16_t address; // from the init routine
    HdHandler handler;
} HdHandlerAddress;

// How the envelope of a voice writes the volume.
typedef enum HdEnvelope {
    HdEnvelope_WriteThenStep, // adds the slope, writes, then counts down and reads the next step
    HdEnvelope_StepThenWrite, // adds the slope, counts down and reads the next step, then writes
} HdEnvelope;

// A replayer version. All addresses are code offsets counted from the init routine: the replayer and its data block
// keep their layout, the header before it moves with the length of the texts.
typedef struct HdDriver {
    uint64_t signature; // FNV-1a of the code and tables from the init routine to the sample table, see hd_module.c
    uint16_t play;
    uint16_t dma_on;
    uint16_t sample_table;
    uint16_t period_table;
    uint16_t command_table; // pattern command routines; the position list routines follow
    uint16_t list_command_table;
    uint16_t envelope_table;
    uint16_t pattern_table_operand;    // the operand of the instruction that loads the pattern table
    uint16_t default_stream_operand;   // the operand of the instruction that sets the first pattern of a voice
    uint16_t default_envelope_operand; // the operand of the instruction that sets the first envelope of a voice
    uint16_t subsong_table_operand;    // the operand of the instruction that loads the subsong table
    uint16_t variables_begin;          // the words the init routine writes (speed, counter, voice mask): not signed
    uint16_t variables_end;
    HdHandlerAddress handlers[HD_MAX_HANDLERS];
    HdEnvelope envelope;
    bool volume_limit;         // the voice has a volume limit, the sample record does not set it
    bool master_limit;         // a word of the replayer (0x40, never written) limits the volume of every voice
    bool loop_from_instrument; // the repeat of a sample is the loop of its record, otherwise silence
    uint8_t list_loop_extra;   // bytes ListLoopBack goes back besides its operand
    uint8_t subsong_stride;    // bytes between the voice pointers of consecutive subsongs
} HdDriver;

typedef struct HdModule {
    const uint8_t* bytes; // the code hunk
    size_t size;
    const HdDriver* driver;
    uint32_t init;         // the init routine: the driver's addresses count from it
    uint32_t* relocations; // sorted code offsets of the 32 bit words that the loader relocates
    size_t relocation_count;
    uint32_t sample_table;
    uint32_t sample_count;
    uint32_t pattern_table;
    uint32_t pattern_count;
    uint32_t envelope_table;
    uint32_t envelope_count;
    uint32_t subsong_table;
    uint32_t subsong_count;
    uint32_t default_stream;
    uint32_t default_envelope;
    uint32_t texts[3]; // title, author, comment
} HdModule;

typedef struct HdVoice {
    NuadePaulaChannel paula;
    const int8_t* sample; // the instrument's sample, AUDxLC of the next note
    uint16_t sample_words;
    const int8_t* repeat; // what the instrument repeats, written after the DMA start
    uint16_t repeat_words;
    uint16_t volume;         // the voice's volume; the envelope adds to it without a limit
    uint16_t base_volume;    // the instrument's volume, what a note starts with
    uint16_t max_volume;     // the voice's volume limit
    uint32_t envelope_start; // the instrument's envelope
    uint32_t envelope_pos;   // the next step of the envelope
    int16_t envelope_slope;  // volume added per tick
    uint16_t envelope_ticks; // ticks until the next step, HD_ENVELOPE_END after the last
    uint32_t list_start;     // the voice's position list
    uint32_t list_pos;       // its next byte
    uint32_t pattern_start;  // the pattern being played, where a repeat goes back to
    uint32_t pattern_pos;    // its next byte
    int16_t repeats_left;    // repeats of the pattern still to play; below 0 = finished
    int8_t rest_ticks; // sequencer steps until the next note; a byte tested as signed, so it wraps (below 0 = now)
    int instrument;    // for the display
} HdVoice;

struct HdPlayer {
    uint8_t* bytes;
    HdModule module;
    uint16_t speed;            // sequencer steps every `speed` ticks
    uint16_t speed_ticks;      // ticks since the last step
    bool voice_on[HD_VOICES];  // the voice mask: all four until a command switches a voice off
    bool song_flag[HD_VOICES]; // the eagleplayer's end flags
    bool fault;                // the data led out of what the port models
    HdVoice voices[HD_VOICES];
};

// The eagleplayer's Check2, plus what this port needs: the file is one executable with a relocation table, its
// replayer is one of the known versions, and every subsong plays to its first end without leaving the data that the
// port models.
bool hd_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `hd_probe` rejects it.
HdPlayer* hd_create(const uint8_t* data, size_t size);
void hd_destroy(HdPlayer* player);

uint32_t hd_subsong_count(const HdPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool hd_start(HdPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
HdTickResult hd_tick(HdPlayer* player, NuadeVoices* voices);

// A text of the header as a C string: up to its terminator or the end of the hunk, cut to `cap` - 1 bytes.
void hd_text(const HdPlayer* player, HdText which, char* out, size_t cap);

// Ticks per sequencer step the replayer starts with.
uint32_t hd_grid_ticks(const HdPlayer* player);

HdVoiceState hd_voice_state(const HdPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
HdPlayer* hd_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
