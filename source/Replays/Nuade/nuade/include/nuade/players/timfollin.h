// SPDX-License-Identifier: MIT
// Tim Follin player (Follin Player II), translated from the UADE eagleplayer "TimFollin". The eagleplayer carries the
// replayer; the modules are Atari ST / Amiga game rips whose song data it relocates and plays.

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

#define TFOL_VOICES 4

// The eagleplayer's file name prefix (the files are also named with it as a suffix).
#define TFOL_EXTENSION "tf"

// The CIA clock: the timebase of TFOL_TICK_LENGTH.
#define TFOL_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / TFOL_TIMEBASE_HZ seconds.
#define TFOL_TICK_LENGTH 14187

// The call stack of the event streams: four entries per voice.
#define TFOL_STACK_PER_VOICE 4
// The call stack of the event streams: four entries per voice (TFOL_STACK_PER_VOICE). The open-time proof
// (tfol_proof.c) shows that no stream calls deeper, returns on an empty stack, runs without a note or leaves the file,
// so the guards below, which stop the voice, are never reached by a module the port opens.
#define TFOL_STACK_ENTRIES (TFOL_STACK_PER_VOICE * TFOL_VOICES)

typedef struct TfolPlayer TfolPlayer;

typedef enum TfolStatus {
    TfolStatus_Ok = 0,
    TfolStatus_InvalidData,
    TfolStatus_OutOfMemory,
} TfolStatus;

typedef struct TfolCreateResult {
    TfolPlayer* player; // 0 on failure
    TfolStatus status;
} TfolCreateResult;

typedef struct TfolTickResult {
    bool ended; // every voice has executed its stop command
} TfolTickResult;

// What voice (0-based) plays: the period of its last note (0 if none yet) and its instrument number.
typedef struct TfolVoiceState {
    uint32_t period;
    int instrument;
} TfolVoiceState;

typedef struct TfolModule {
    uint8_t* mem;      // the module as the replayer sees it: the sweeps write into it, Paula plays from it
    uint8_t* original; // the file bytes, copied back into mem at every start
    size_t size;
    uint32_t table;         // file offset of the song table
    uint32_t address_delta; // original address + address_delta = file offset (32-bit wrap, like adda.l)
    uint32_t subsong_count;
} TfolModule;

typedef struct TfolSong {
    uint32_t stream[TFOL_VOICES]; // file offset of each voice's event stream
    uint32_t instrument_table;    // file offset of the table of instrument addresses
    uint32_t voices;              // 4, or TFOL_MIN_VOICES for a song without the fourth voice
} TfolSong;

typedef struct TfolInstrument {
    bool valid;            // the record lies inside the module
    uint32_t offset;       // file offset of the record
    uint32_t sample;       // file offset of the sample, AUDxLC
    uint16_t length_words; // AUDxLEN
    uint32_t sample_bytes; // what AUDxLEN plays: 0 is 65536 words
} TfolInstrument;

typedef struct TfolVoice {
    NuadePaulaChannel paula;

    bool active; // the voice plays its stream
    uint32_t stream;
    uint32_t loop_start;
    uint32_t stack_top; // index into the player's call stack

    bool has_instrument;
    TfolInstrument instrument;
    uint8_t instrument_number;
    bool note_off;               // DMA is switched off one tick before the note ends
    bool second_sample;          // a note is followed by the second sample
    uint8_t second_sample_entry; // its entry in the instrument table

    uint8_t note_counter; // ticks left of the note
    uint8_t note_length;  // the length of every note, 0: the stream gives it
    uint8_t note;         // the note number, index of the period table
    uint16_t period;
    uint8_t transpose;
    bool skip_transpose; // the next note ignores the transpose

    uint8_t loop_count;

    uint8_t level; // amplitude, written to AUDxVOL
    uint8_t envelope_step;
    uint8_t envelope_level_start;
    uint8_t envelope_sustain;
    uint8_t envelope_attack_rate;
    uint8_t envelope_release_rate;
    bool envelope_restart; // a note restarts the envelope
    uint8_t envelope_state_start;
    uint8_t envelope_state;
    int8_t envelope_counter;

    uint8_t portamento_rate;
    uint8_t portamento_target;

    uint8_t vibrato_depth;
    uint8_t vibrato_up_ticks;
    uint8_t vibrato_down_ticks;
    uint8_t vibrato_counter;
    uint8_t vibrato_direction; // shared with the pitch modulation

    uint8_t pitch_modulation_delay; // also the enable
    uint8_t pitch_modulation_delay_counter;
    uint8_t pitch_modulation_amount;
    uint8_t pitch_modulation_ticks;
    uint8_t pitch_modulation_counter;
    uint8_t pitch_modulation_direction_start;

    uint8_t sweep_rate;
    uint8_t sweep_counter;
    uint8_t sweep_position;
    bool sweep_down;

    bool retrigger; // the second sample starts at the next tick
} TfolVoice;

struct TfolPlayer {
    TfolModule module;
    uint32_t song;
    TfolSong song_record;
    TfolVoice voices[TFOL_VOICES];
    uint32_t stack[TFOL_STACK_ENTRIES];
};

// The eagleplayer's check (Check2) plus the relocation InitPlayer needs: true when the eagleplayer plays the module.
bool tfol_probe(const uint8_t* data, size_t size);

TfolCreateResult tfol_create(const uint8_t* data, size_t size);
void tfol_destroy(TfolPlayer* player);

// The eagleplayer's SubSongRange: the number of 68k jumps that start the module, at least 1.
uint32_t tfol_subsong_count(const TfolPlayer* player);

// (Re)start `subsong` (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool tfol_start(TfolPlayer* player, uint32_t subsong, NuadeVoices* voices);

TfolTickResult tfol_tick(TfolPlayer* player, NuadeVoices* voices);
TfolVoiceState tfol_voice_state(const TfolPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
TfolPlayer* tfol_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
