// SPDX-License-Identifier: MIT
// Jason C. Brooke player (Gremlin / Ocean / US Gold game sound drivers, 1987-90), translated from the UADE eagleplayer
// "Jason C. Brooke player" (Wanted Team). The replayer is not in the eagleplayer: every module is the game's own
// driver (68k code, voice records, tables and song data in one file) and the eagleplayer finds its entry points and
// tables by scanning the code. The player does the same, then runs the replayer logic of the version it found.

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

#define JB_VOICES 4

// The CIA clock: the timebase of JB_TICK_LENGTH.
#define JB_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / JB_TIMEBASE_HZ seconds.
#define JB_TICK_LENGTH 14187

#define JB_A_MAX_COMMANDS 32

typedef struct JbPlayer JbPlayer;

typedef struct JbTickResult {
    bool looped; // every voice wrapped its track list since the last time they all had
    bool ended;  // the stop command ran: the driver plays nothing more
} JbTickResult;

typedef struct JbVoiceState {
    int note;   // the note byte of the voice's last note, -1 if none yet
    int sample; // sample table index the voice plays
} JbVoiceState;

typedef enum JbVersion {
    JbVersion_A,
    JbVersion_B,
} JbVersion;

typedef struct JbModule {
    uint8_t* data;
    uint32_t size;
} JbModule;

typedef enum JbACommand {
    JbACommand_Invalid = 0, // not one of the known handlers: the driver would run unknown code
    JbACommand_Rest,        // key off: sample 0, DMA restart
    JbACommand_ClearFlags,
    JbACommand_Slide,    // pitch slide: step, delay
    JbACommand_NoteDown, // the note number counts down on the ticks between steps
    JbACommand_NoteUp,
    JbACommand_NextTrack,
    JbACommand_Vibrato,
    JbACommand_Stop,
    JbACommand_Transpose,
    JbACommand_AdkCon,
    JbACommand_Tie, // no DMA restart on the next note
    JbACommand_TieNoEnvelope,
    JbACommand_NoEnvelope, // keep the volume envelope on the next note
    JbACommand_PwmSet,
    JbACommand_Volume,
    JbACommand_SkipByte, // a byte the game reads back (a sync point); the song ignores it
    JbACommand_GlobalTranspose,
    JbACommand_Tempo,
    JbACommand_ArpProgram,
    JbACommand_LoopStart,
    JbACommand_LoopTwice,
    JbACommand_LoopEnd,
} JbACommand;

// Offsets into the module of everything the replayer reads.
typedef struct JbATables {
    uint32_t globals;          // the driver's global bytes, see jb_a_globals
    uint32_t pwm;              // the waveform the PWM step rewrites
    uint32_t envelope;         // volume envelope steps: (delta, duration) pairs
    uint32_t pitch_sequences;  // signed offsets added to the note; index = position + 1
    uint32_t sequence_starts;  // new driver: start position of each pitch sequence (indexed by the program number)
    uint32_t periods;          // word table, indexed by 2 * note
    uint32_t samples;          // JB_A_SAMPLE_SIZE bytes each, see jb_a_sample
    uint32_t silence_address;  // the sample a one-shot repeats into
    uint32_t silence_length;   // the length word read for it
    uint32_t sequence_codes;   // 0x90..0x9f bytes: pitch sequence loop start
    uint32_t envelope_start;   // envelope number -> position in the envelope table
    uint32_t envelope_volume;  // the same table, offset to the first volume of an envelope
    uint32_t songs;            // JB_A_SONG_SIZE bytes each, see jb_a_song
    uint32_t pwm_length;       // the length word of the PWM sample, which a command rewrites
    uint32_t global_transpose; // byte, 0 when the driver has none
    uint32_t commands;         // word table of command handlers
} JbATables;

typedef struct JbAConfig {
    JbATables tables;
    uint8_t note_limit;       // bytes below this are notes (old syntax) or row lengths (new syntax)
    bool new_syntax;          // 0x41 style note and command bytes
    bool pwm_noise;           // the PWM step writes a shifting noise pattern instead of sweeping a pulse width
    bool sequence_mask_form;  // a negative pitch sequence byte loops with its low 7 bits as the value
    bool pitch_uses_sequence; // the pitch sequence value is added to the note
    uint8_t command_count;
    JbACommand commands[JB_A_MAX_COMMANDS];
    int subsong_count;
} JbAConfig;

typedef struct JbAGlobals {
    uint8_t play;      // sequencer and voices run
    uint8_t master;    // volume scale (0xFF)
    uint8_t slow_down; // added to an accumulator each tick; the tick is skipped on a carry
    uint8_t slow_acc;
    uint8_t tempo; // added to an accumulator each tick; the sequencer steps on a carry
    uint8_t tempo_acc;
    uint8_t pwm_index; // PWM step
    uint8_t pwm_low;
    uint8_t pwm_high;
    uint8_t pwm_phase;  // direction (sweep) or seed (noise)
    uint8_t pwm_phase2; // direction (noise)
    uint8_t transpose;  // added to every note (the game sets it)
} JbAGlobals;

typedef struct JbAVoice {
    uint8_t flags; // the JbAFlag bits (jb_a.c)
    uint8_t track_position;
    uint16_t stream; // offset of the next byte of the current track
    uint16_t track_list;
    uint8_t track_end;
    uint8_t track_loop;
    int16_t slide_step;
    uint16_t slide_sum;
    uint8_t slide_delay;
    uint8_t envelope_position;
    uint8_t envelope_number;
    uint8_t envelope_timer;
    uint8_t envelope_volume;
    uint8_t volume;
    uint8_t row_counter;
    uint8_t row_length;
    uint8_t note;
    uint8_t vibrato_depth;
    uint8_t vibrato_speed;
    uint8_t vibrato_sum;
    uint8_t transpose;
    uint8_t sample_next; // the sample the next note plays
    uint8_t sample;
    uint8_t sequence_loop; // old driver: loop start of the pitch sequence
    uint8_t sequence_position;
    uint8_t sequence_program; // new driver
    uint8_t loop_count;
    uint16_t loop_position;
    uint8_t dma_phase; // 2 after a note: the next tick starts the sample, the one after sets its loop
    bool note_started; // a note has started on the voice (for the display)
    bool halted;       // the track data is malformed: the voice stops stepping

    NuadePaulaChannel paula;
} JbAVoice;

typedef struct JbA {
    JbAConfig config;
    JbAGlobals globals;
    JbAVoice voices[JB_VOICES];
    bool wrapped[JB_VOICES]; // the voice passed its track list end since all of them last did
    bool looped;
    bool stopped;
} JbA;

typedef struct JbBTables {
    uint32_t master; // volume scale byte, 0 when the driver has none
    uint32_t pitch_sequences;
    uint32_t envelope_start; // envelope number -> position in the envelope data
    uint32_t envelope;       // envelope data
    uint32_t periods;
    uint32_t sequence_codes; // 0x90..0x9f bytes: pitch sequence loop start
    uint32_t blocks;         // sample bank, 128 bytes per block
    uint32_t songs;          // JB_B_SONG_SIZE bytes each, see jb_b_song
} JbBTables;

typedef struct JbBConfig {
    JbBTables tables;
    bool envelope_wraps; // the envelope data index is a byte (start + position wraps at 256)
    bool vibrato_hold;   // the vibrato holds its depth on the odd ticks; its command sets that mode from the depth
    int subsong_count;
} JbBConfig;

typedef struct JbBVoice {
    uint8_t flags;
    uint8_t track_position;
    uint16_t stream;
    uint16_t track_list;
    int8_t slide_step;
    uint8_t slide_delay;
    uint16_t slide_sum;
    uint8_t row_counter;
    uint8_t row_length;
    uint8_t note;
    uint8_t volume;
    uint8_t envelope_position;
    uint8_t envelope_volume;
    uint8_t envelope_number;
    uint8_t vibrato_depth;
    uint8_t vibrato_speed;
    uint8_t vibrato_sum;
    uint8_t transpose;
    uint8_t block;
    uint8_t sequence_loop;
    uint8_t sequence_position;
    uint16_t period;
    bool note_started; // a note has started on the voice (for the display)
    bool halted;       // the track data is malformed: the voice stops stepping

    NuadePaulaChannel paula;
} JbBVoice;

typedef struct JbB {
    JbBConfig config;
    JbBVoice voices[JB_VOICES];
    bool running;
    uint8_t tempo;
    uint8_t counter;
    bool wrapped[JB_VOICES];
    bool looped;
    bool stopped;
} JbB;

struct JbPlayer {
    JbModule original; // as loaded
    JbModule work;     // the copy the replayer runs on (padded for Paula): the driver rewrites its own tables
    JbVersion version;
    JbA a;
    JbB b;
    const char* driver_name;
};

// The eagleplayer's Check2: the code starts with the old driver's entry sequence, or the new driver's entry sequence
// (CIA bit, interrupt disable, a pc-relative pointer back to the file start) sits within the first 0xE0 bytes.
bool jb_probe(const uint8_t* data, size_t size);

// Copies the module and locates the replayer's tables in its code; 0 when the code is not one of the known drivers.
JbPlayer* jb_create(const uint8_t* data, size_t size);
void jb_destroy(JbPlayer* player);

int jb_subsong_count(const JbPlayer* player);
// The driver found in the code ("driver A", "driver A, new syntax", "driver A, noise", "early driver (B)"), for the
// metadata.
const char* jb_driver_name(const JbPlayer* player);

// (Re)start a subsong (0-based) from its beginning.
bool jb_start(JbPlayer* player, int subsong);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
JbTickResult jb_tick(JbPlayer* player, NuadeVoices* voices);

// Ticks per sequencer step, rounded: version A adds its tempo byte to an accumulator each tick and steps on a carry
// (256 / tempo ticks), version B steps when a counter that starts at its tempo runs out.
int jb_ticks_per_step(const JbPlayer* player);

JbVoiceState jb_voice_state(const JbPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
JbPlayer* jb_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
