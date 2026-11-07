// SPDX-License-Identifier: MIT
// Fred Gray player (Denton Designs / Ocean game music: Eco, Black Lamp, Star Goose, Victory Road), translated from the
// UADE eagleplayer "FredGray" (Wanted Team). A module is an Amiga executable that holds the game's own music code: a
// small sequencer whose song sections are 68k routines. The eagleplayer loads the file, patches the note start and
// calls the module's interrupt and init routines. The player does what the eagleplayer does to the code, finds the
// engine and decodes the song sections from the code, then runs the engine of the version it found in C.

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

// Paula channels the module writes (it uses channels 0..2; channel 3 stays idle)
#define FG_VOICES 4

// The CIA clock: the timebase of FG_TICK_LENGTH.
#define FG_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / FG_TIMEBASE_HZ seconds.
#define FG_TICK_LENGTH 14187

// Voices the module drives; channel 3 is never written
#define FG_ACTIVE_VOICES 3

typedef struct FgPlayer FgPlayer;

typedef struct FgTickResult {
    bool looped; // the song reached its end section: the module calls the eagleplayer's SongEnd from now on
} FgTickResult;

typedef struct FgVoiceState {
    int note; // the note index of the voice's last note, -1 if none yet
} FgVoiceState;

typedef struct FgModule {
    uint8_t* image;
    uint32_t size;
    uint32_t* relocs; // offsets of the longs the hunk relocates (absolute addresses), ascending
    uint32_t reloc_count;
} FgModule;

typedef struct FgInstrument {
    uint8_t vibrato_row; // added to the vibrato phase: selects 16 vibrato bytes
    uint8_t vibrato_delay;
    uint8_t envelope_offset;
    uint8_t envelope_speed; // ticks per envelope step
} FgInstrument;

typedef struct FgCommand {
    uint16_t pitch;
    uint16_t target;
    uint16_t step;
    uint16_t duration;
    FgInstrument instrument;
    uint8_t next_command;
    uint8_t flags;
} FgCommand;

typedef enum FgVersion {
    FgVersion_Eco = 0,
    FgVersion_BlackLamp,
    FgVersion_StarGoose,
    FgVersion_VictoryRoad,
    FgVersion_Count,
} FgVersion;

typedef struct FgFeatures {
    uint32_t voice_stride; // bytes of a voice record
    uint16_t note_mask;    // the note plus the track's offset is ANDed with this: FG_NOTE_MASK_*
    bool step_preloaded;   // the interrupt loads the section position into the step before the request stage
    bool has_transpose;    // notes add a transpose that the sections set and a request clears
    bool has_cap;          // the volume envelope is capped by a variable
    bool has_noise;        // a 16 byte wave changes every tick
    bool wave_select;      // a command picks the wave the voice's Paula channel loops
    bool has_kick;         // commands switch channels between silence and a 256 byte sample
    bool has_rest;         // a note byte with bit 6 set is a rest
    bool gated_setup;      // the instrument setup runs only when requested or at the start of a section
    bool request_len;      // a new request sets AUDxLEN of one voice
    bool has_mask;         // a channel mask routine starts commands from a position-indexed table
    bool mask_sets_setup;  // the channel mask result is the gate of the instrument setup
} FgFeatures;

typedef enum FgTest {
    FgTest_None = 0,
    FgTest_Zero,     // step == 0
    FgTest_NonZero,  // step != 0
    FgTest_Equal,    // step == value
    FgTest_NotEqual, // step != value
    FgTest_AtLeast,  // step >= value (unsigned)
    FgTest_BitSet,   // bit `value` of the step is set
    FgTest_BitClear, // bit `value` of the step is clear
} FgTest;

typedef enum FgOpKind {
    FgOp_Tempo = 0,        // value: ticks per step. Between the steps the section ends here for the tick
    FgOp_Instruments,      // instrument[0..2]: offsets into the instrument table for voices 0..2
    FgOp_Track,            // voice, table, offset, nibble: a note from table[step]
    FgOp_Command,          // voice, value: sets the voice's command
    FgOp_CommandFromTable, // voice, table: the voice's command is table[step]
    FgOp_SlideStep,        // voice, value
    FgOp_SetTranspose,     // value
    FgOp_AddTranspose,     // value
    FgOp_XorTranspose,     // value
    FgOp_SetNextState,     // value: the section to start next tick
    FgOp_ClearState,       // the current section becomes 0 (the song end)
    FgOp_LoadStep,         // step = position
    FgOp_StepAnd,          // step &= value
    FgOp_StepShift,        // step >>= value
    FgOp_Branch,      // test, value, then_shift, jump: jumps when the test holds, then shifts the step by then_shift
    FgOp_Jump,        // always to `jump`
    FgOp_ChannelMask, // value: starts the commands the mask table allows at this position
    FgOp_SetLc,       // voice, table: AUDxLC of the voice (Eco sections)
    FgOp_SetLen,      // voice, value: AUDxLEN
    FgOp_Reset,       // every voice gets the silent command
    FgOp_CapFromStep, // volume cap from the step
    FgOp_CapFromPosition, // volume cap from the position
    FgOp_End,             // end, value
} FgOpKind;

typedef enum FgEnd {
    FgEnd_Return = 0, // the section is done for this tick
    FgEnd_Advance,    // count the step: at the end of the section the next section follows
    FgEnd_AdvanceTo,  // count the step: at the end of the section the section `value` follows
    FgEnd_Reset,      // reset the voices
} FgEnd;

typedef struct FgOp {
    FgOpKind kind;
    FgTest test; // Branch
    FgEnd end;   // End
    uint8_t voice;
    uint8_t nibble;     // Track: the note byte also selects the vibrato
    uint8_t then_shift; // Branch: bits the step is shifted right by after the test (lsr.w #n,d7; bcs)
    uint16_t value;
    uint16_t jump;
    uint32_t table;
    uint16_t offset; // Track: the word the track routine adds to the note (a sign extended moveq, or a long's low word)
    uint8_t instrument[FG_ACTIVE_VOICES];
} FgOp;

typedef struct FgSection {
    uint32_t first; // index of the first operation in the layout's pool
    uint32_t count;
} FgSection;

typedef struct FgInit {
    uint32_t subsong_map;          // one byte per subsong: the section request the subsong starts with
    uint32_t lc[FG_ACTIVE_VOICES]; // AUDxLC the init routine writes
    uint16_t len_words;            // AUDxLEN of every voice
    bool writes_period;
    uint16_t period;
} FgInit;

// The channel mask routine: ANDs its argument with a byte of `table` picked by the position; a non-zero result starts
// `command_value` on `command_voice` and, in the Eco version, writes AUDxLEN of that voice
typedef struct FgChannelMask {
    uint32_t table;
    uint16_t index_mask; // the position is ANDed with this to index the table
    uint8_t command_voice;
    uint8_t command_value;
    bool writes_len;
    uint16_t len_off; // AUDxLEN before the test
    uint16_t len_on;  // AUDxLEN when the result is non-zero
} FgChannelMask;

typedef struct FgLayout {
    FgVersion version;
    FgFeatures features;
    uint32_t voice_base;      // the voice records, as the module initialises them
    uint32_t section_lengths; // one word per section: the steps it lasts
    uint32_t vibrato;         // signed bytes, 16 per vibrato row
    uint32_t envelope;        // volume envelopes
    uint32_t periods;         // 48 words: 4 steps per semitone
    uint32_t commands;        // 16 byte records
    uint32_t instruments;     // 4 byte records
    FgChannelMask mask;       // has_mask
    uint32_t wave_base;       // wave_select: the first wave; wave n starts n * 16 bytes on
    uint32_t noise_wave;      // has_noise: the 16 bytes that change every tick
    uint32_t noise_table;     // has_noise: the 16 + 15 bytes they are made of
    uint32_t noise_phase;     // has_noise: the word that counts the changes
    uint32_t kick_silence;    // has_kick: the wave of a silent channel
    uint32_t kick_sample;     // has_kick: the 256 byte sample
    uint8_t request_len_voice;
    uint16_t request_len_words;
    FgInit init;
    FgSection* sections; // one per section number; section 0 is the song end
    uint32_t section_count;
    FgOp* op_pool;
} FgLayout;

typedef struct FgVoice {
    uint16_t period;       // the engine period; the register gets twice this
    uint16_t note_period;  // the register period of the last note (for the display)
    uint8_t envelope_step; // counts down from 15
    uint8_t volume;
    uint8_t channel_shift; // the channel number: a command's flags move by it into the kick mask
    uint8_t envelope_tick;
    FgCommand active;                     // the loaded command; the slide changes its pitch
    bool kick_bits[2 * FG_ACTIVE_VOICES]; // the kick mask bits the voice sets (skips) when it loads a command
    uint8_t command;
    int16_t vibrato;
    uint8_t vibrato_left;
    uint8_t vibrato_phase;
    bool kick_sample; // the channel plays the 256 byte sample
    NuadePaulaChannel paula;
} FgVoice;

typedef struct FgSong {
    uint8_t request;
    uint8_t next_state;
    uint8_t state;
    uint8_t tempo_counter;
    uint16_t position;
    uint16_t transpose;
    uint8_t cap;
    bool kick_skip[2 * FG_ACTIVE_VOICES]; // per voice: skip the switch to silence, skip the switch to the sample
    bool setup_pending;
} FgSong;

struct FgPlayer {
    FgModule original; // as loaded
    FgModule work;     // the copy the engine runs on: the noise and the voices' waves live in it
    FgLayout layout;
    FgVoice voices[FG_VOICES];
    FgSong song;
    bool song_end_seen;
};

// The eagleplayer's Check2 plus the engine check: true only for a module this player can run.
bool fg_probe(const uint8_t* data, size_t size);

// Copies the module, finds the engine and decodes the song sections; 0 when the code is not one of the known engines.
FgPlayer* fg_create(const uint8_t* data, size_t size);
void fg_destroy(FgPlayer* player);

int fg_subsong_count(const FgPlayer* player);
// "Eco", "Black Lamp", "Star Goose" or "Victory Road": the engine version found in the code.
const char* fg_version_name(const FgPlayer* player);
// The module header's title and composer strings
const char* fg_title(const FgPlayer* player);
const char* fg_author(const FgPlayer* player);

// (Re)start a subsong (0-based) from its beginning: the module's init routine, with the DMA on.
bool fg_start(FgPlayer* player, int subsong, NuadeVoices* voices);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
FgTickResult fg_tick(FgPlayer* player, NuadeVoices* voices);

FgVoiceState fg_voice_state(const FgPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
FgPlayer* fg_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
