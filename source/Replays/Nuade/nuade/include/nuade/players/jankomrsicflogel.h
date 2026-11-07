// SPDX-License-Identifier: MIT
// Janko Mrsic-Flogel player, translated from the UADE eagleplayer "JankoMrsicFlogel" (Wanted Team) and the replayers
// of the modules. A song is an Amiga executable of one code hunk: the "J.FLOGEL" mark, the entry points of the
// replayer, the title texts, the waveforms, the note streams of the three voices and the replayer itself. The
// eagleplayer loads the file as a program and patches it; the replayer comes from the module. Four versions are
// known (Bomb Jack, Fire Blaster, Luxor, Protector with its two siblings).

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

#define JMF_VOICES 4

// The CIA clock: the timebase of JMF_TICK_LENGTH.
#define JMF_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / JMF_TIMEBASE_HZ seconds.
#define JMF_TICK_LENGTH 14187

typedef struct JmfPlayer JmfPlayer;

typedef struct JmfTickResult {
    bool looped; // a note stream reached its end and the replayer started over: the eagleplayer calls SongEnd
} JmfTickResult;

typedef enum JmfText {
    JmfText_Title,
    JmfText_Artist,
    JmfText_Message,
    JmfText_Count,
} JmfText;

typedef enum JmfAction {
    JmfAction_WaveA, // load the first waveform, then play the note
    JmfAction_WaveB, // load the second waveform, then play the note
    JmfAction_Note,  // play the note with the waveform the voice has
    JmfAction_Skip,  // the byte after the duration is ignored
    JmfAction_Stop,  // DMA off, no byte follows
} JmfAction;

typedef struct JmfVoice {
    NuadePaulaChannel paula;
    uint32_t stream_start; // the note stream the song starts over with
    uint32_t stream_pos;   // its next byte
    uint8_t counter;       // steps until the next event
    JmfAction action;      // what the mode adds to an event
} JmfVoice;

// A replayer version. Offsets count from the play routine. The code from there to the period table is identical
// within a version; the waveform and table addresses are read from the module.
typedef struct JmfDriver {
    uint64_t signature; // FNV-1a of the replayer's code, see jmf_module.c
    uint32_t length;    // bytes of code in the signature: from the play routine to the period table
    uint32_t reinit;    // the routine that starts the song over; the init routine ends with a branch to it
    uint8_t tick_divisor;
    int8_t note_offset;          // added to a note byte to index the period table
    uint16_t wave_a_words;       // AUDxLEN with the first waveform
    uint16_t wave_b_words;       // AUDxLEN with the second waveform, 0 = the register is left alone
    uint8_t wave_b_period_shift; // the second waveform plays at the table period shifted left
    uint16_t idle_words;         // AUDxLEN the song start gives the fourth voice, 0 = not written
} JmfDriver;

typedef struct JmfModule {
    const uint8_t* bytes; // the code hunk
    size_t size;
    const JmfDriver* driver;
    uint32_t* relocations; // sorted code offsets of the 32 bit words that the loader relocates
    size_t relocation_count;
    uint32_t play;
    uint32_t init;
    uint32_t subsong_count;
    uint32_t texts[JmfText_Count]; // title, author, comment
    uint32_t* streams;             // start of the note stream of each voice of each subsong, subsong * 3 + voice
    uint32_t wave_a;               // waveform offsets, as AUDxLC takes them
    uint32_t wave_b;
    uint32_t idle_wave;
    uint32_t period_table;
    uint32_t period_count;
    uint32_t variables; // the replayer's variables: the counters, the modes and the stream pointers
    uint32_t variables_end;
    uint32_t stream_variables[3]; // where the init routine stores the note streams: the start-over routine's sources
} JmfModule;

struct JmfPlayer {
    uint8_t* chip; // the file with the zeros that Paula reads past the hunk
    JmfModule module;
    uint8_t divider; // ticks until the next step
    JmfVoice voices[JMF_VOICES];
};

// The eagleplayer's Check2, plus what this port needs: the file is one executable with a relocation table, its
// replayer is one of the known versions, and every note stream of every subsong stays inside the data that the port
// models.
bool jmf_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `jmf_probe` rejects it.
JmfPlayer* jmf_create(const uint8_t* data, size_t size);
void jmf_destroy(JmfPlayer* player);

uint32_t jmf_subsong_count(const JmfPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool jmf_start(JmfPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
JmfTickResult jmf_tick(JmfPlayer* player, NuadeVoices* voices);

// A text of the header as a C string: up to its terminator or the end of the hunk, cut to `cap` - 1 bytes.
void jmf_text(const JmfPlayer* player, JmfText which, char* out, size_t cap);

// Ticks per sequencer step.
uint32_t jmf_grid_ticks(const JmfPlayer* player);

// The period of the last note a voice started, 0 if none.
uint16_t jmf_voice_period(const JmfPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
JmfPlayer* jmf_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
