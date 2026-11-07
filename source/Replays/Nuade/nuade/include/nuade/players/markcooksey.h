// SPDX-License-Identifier: MIT
// Mark Cooksey player for the Amiga modules that carry their replayer ("*.mc"). The old ST format is markcooksey_old.

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

#define MC_VOICES 4

// The CIA clock: the timebase of MC_TICK_LENGTH.
#define MC_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / MC_TIMEBASE_HZ seconds.
#define MC_TICK_LENGTH 14187

#define MC_STACK_FRAMES 4 // the voice record has room for four frames below its length counter

typedef struct McPlayer McPlayer;

typedef struct McTickResult {
    bool looped; // every voice has run an end or jump command (the eagleplayer's SongEnd)
    bool ended;  // no voice is running any more
} McTickResult;

typedef struct McVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // instrument number of the last note started, -1 if none
} McVoiceState;

typedef enum McFlavor {
    // Commando, Ghosts 'n Goblins, Paperboy, Grand National.
    McFlavor_Early,
    // Gremlins 2, Mighty Bomb Jack, World Championship Soccer: two more leas (of a volume table) before the period
    // table in the tick, and the tick looks at the stream before it counts the length down.
    McFlavor_Late,
} McFlavor;

typedef struct McModule {
    uint8_t* data; // the file in a nuade_paula_padded_copy: a sample may run past its end into zeros
    size_t size;   // without the padding
    McFlavor flavor;
    uint32_t subsongs; // offset of the subsong table
    int subsong_count;
    uint32_t sequences;   // offset of the sub-sequence table
    uint32_t instruments; // offset of the instrument table
    uint32_t periods;     // offset of the period table
    uint32_t samples;     // offset of the sample data
    uint32_t state;       // offset of the replayer's variables
    int voice_stride;     // bytes between the voice records of the variables
} McModule;

// An instrument record: four longs (offsets), four words, two bytes, a word.
typedef struct McInstrument {
    uint32_t sample;          // offset of the first block, 0: the note only retunes the running sample
    uint32_t loop;            // offset of the block the sample repeats, 0: it repeats silence
    uint32_t volume_envelope; // offset of a list of levels, ended by 0; 0: none
    uint32_t pitch_envelope;  // offset of a list of signed pitch steps, ended by 0x80; 0: none
    uint16_t sample_words;
    uint16_t loop_words;
    uint16_t period_scale;        // the period is multiplied by this / 0x10000 for a sample start
    uint16_t period_scale_retune; // and by this when the note only retunes
    uint8_t volume_envelope_speed;
    uint8_t pitch_envelope_speed;
    uint16_t gain; // multiplies the voice volume
} McInstrument;

typedef struct McStackFrame {
    int repeats_left;
    uint32_t loop_start; // offset of the sub-sequence
    uint32_t resume;     // offset after the call
} McStackFrame;

typedef struct McVoice {
    uint32_t stream; // offset of the next command, 0: the voice has ended
    uint16_t length; // ticks until the next command
    McStackFrame stack[MC_STACK_FRAMES];
    int depth;
    int16_t transpose; // in notes
    bool glide;
    uint16_t glide_ticks;
    uint16_t volume_scale; // set by McCommand_Volume
    int instrument_number;
    McInstrument instrument;
    bool retune_only; // the last note did not start a sample
    uint16_t pitch;   // current pitch (steps), moves towards `target_pitch` when gliding
    uint16_t target_pitch;
    uint16_t start_pitch;
    int16_t glide_step;
    uint32_t volume_envelope_at; // offset of the next level, 0: none
    uint8_t volume_envelope_wait;
    uint32_t pitch_envelope_at; // offset of the next step, 0: none
    uint8_t pitch_envelope_wait;
    bool reached_end; // the voice ran an end or jump command since the song last ended
    int shown_note;
    NuadePaulaChannel paula;
} McVoice;

struct McPlayer {
    McModule module;
    McVoice voices[MC_VOICES];
    bool song_ended; // set when the last voice reaches its end or jump command (the eagleplayer's SongEnd)
};

// The eagleplayer's Check2: the replayer's entry table and the first lea of the replayer.
bool mc_probe(const uint8_t* data, size_t size);

// Locates the tables of the replayer. 0 when the module is not one the eagleplayer plays.
McPlayer* mc_create(const uint8_t* data, size_t size);
void mc_destroy(McPlayer* player);

int mc_subsong_count(const McPlayer* player);

// (Re)start a subsong from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mc_start(McPlayer* player, int subsong, NuadeVoices* voices);

McTickResult mc_tick(McPlayer* player, NuadeVoices* voices);

McVoiceState mc_voice_state(const McPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
McPlayer* mc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
