// SPDX-License-Identifier: MIT
// Medley player (Paul van der Valk, "MSO" files), translated from the UADE eagleplayer "Medley". A song is one file:
// the "MSOB" mark and four tables (songs, tracks, instruments, waves). A song gives each of the four voices a track
// (a list of notes and commands), an instrument and a volume; an instrument is a sample, a looped sample or a
// waveform synthesised from a wave by adding two reads of it, with an envelope and up to four modulators. The
// eagleplayer holds the replayer.

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

#define MDL_VOICES 4

// The CIA clock: the timebase of MDL_TICK_LENGTH.
#define MDL_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MDL_TIMEBASE_HZ seconds.
#define MDL_TICK_LENGTH (5 * 709379 / 219) // rePlayer "arbitrary tempo to match the proper replay from deliplayer)

#define MDL_MODULATORS 4
// Calls and loops nest this deep; one more does nothing.
#define MDL_STACK_DEPTH 8
#define MDL_REGIONS 8
#define MDL_VOLUME_STEPS 64

typedef struct MdlPlayer MdlPlayer;

typedef struct MdlTickResult {
    bool looped; // a voice reached the end of its track: the eagleplayer calls SongEnd
    bool ended;  // the song read memory the port does not model: later ticks do nothing
} MdlTickResult;

typedef struct MdlVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // that voice's instrument number, -1 if none
} MdlVoiceState;

typedef enum MdlTable {
    MdlTable_Song = 0,
    MdlTable_Track,
    MdlTable_Instrument,
    MdlTable_Wave,
    MdlTable_Count
} MdlTable;

// The module as the replayer sees its memory. Every read goes through the readers below; a read outside the file sets
// `unmapped`, because the port does not model what Amiga memory holds there.
typedef struct MdlModule {
    uint8_t* bytes; // the file followed by zeros (nuade_paula_padded_copy)
    uint32_t size;
    uint32_t base[MdlTable_Count]; // the first entry of each table; the byte before it holds the highest index
    uint16_t subsong_count;
    bool unmapped;
} MdlModule;

// A song: four voices with a track, an instrument and a volume each, and the settings all of them share.
typedef struct MdlSong {
    uint8_t track[MDL_VOICES];            // track number, 0 = the voice is not used
    uint8_t instrument[MDL_VOICES];       // 0 = default_instrument
    uint8_t default_instrument;           // 0 = instrument 1
    int8_t transpose;                     // added to every note
    uint8_t speed;                        // ticks between two voice updates, minus one
    uint8_t arpeggio;                     // the arpeggio table
    uint8_t arpeggio_speed;               // ticks per arpeggio frame, 0 = off
    uint8_t master_volume;                // 0..63
    uint8_t random_wait_min[MDL_VOICES];  // 0 = the voice plays its track; else it restarts at random
    uint8_t random_wait_mask[MDL_VOICES]; // the random part of the wait, as a mask plus one
    uint8_t voice_volume[MDL_VOICES];     // 0..63
} MdlSong;

typedef enum MdlInstrumentType {
    MdlInstrumentType_Sample = 0,  // the whole wave, looped
    MdlInstrumentType_Looped = 1,  // a window of the wave, moved by a modulator
    MdlInstrumentType_Synth = 2,   // two reads of the wave added into a buffer
    MdlInstrumentType_OneShot = 3, // the whole wave at the note start, then silence
} MdlInstrumentType;

typedef struct MdlModulatorSetup {
    uint8_t kind;       // MdlModulatorKind
    bool gate;          // the output is the full level or nothing
    bool free_running;  // keeps its level at a note start
    bool one_shot;      // stays down after the fall instead of rising again
    bool invert;        // the output is level - value
    uint8_t delay;      // ticks before it moves
    uint8_t top_start;  // 0, or the voice flag (1 or 2) that starts it at the top
    uint8_t half_start; // 0, or the voice flag (1 or 2) that starts it at half level
    uint16_t level;     // the top
    uint16_t rise;      // per update
    uint16_t fall;      // per update
} MdlModulatorSetup;

typedef struct MdlInstrument {
    MdlInstrumentType type; // a value above OneShot acts as OneShot
    uint8_t window;         // Looped: the start of the window
    uint8_t phase;          // Synth: the start of the first read
    int8_t transpose;
    uint8_t wave[MDL_REGIONS]; // the wave of each note range, 0 = the one below
    uint8_t tone;              // Synth: the steps of the two reads, a nibble each
    uint16_t max_level;        // the envelope
    uint16_t sustain_level;
    uint16_t attack;
    uint16_t decay;
    uint16_t release;
    MdlModulatorSetup modulator[MDL_MODULATORS];
} MdlInstrument;

typedef struct MdlWave {
    uint16_t length;  // bytes of data
    uint8_t octave;   // the octave of the data's pitch
    bool second_half; // Looped: the window can be in the second half
    uint32_t data;    // the offset of the first byte
} MdlWave;

typedef struct MdlModulatorState {
    uint16_t value;
    uint8_t delay;
    bool falling;
} MdlModulatorState;

typedef struct MdlVoice {
    NuadePaulaChannel paula;
    int index;
    int8_t* wave_buffer; // this voice's part of the chip memory
    bool active;
    bool sequence_running;
    uint32_t sequence; // the next byte of the track
    uint32_t restart;  // where the track starts
    uint8_t duration;  // ticks left of the note
    uint8_t divider;   // counts the ticks between two updates
    uint8_t note;
    bool key_on;
    bool retrigger; // the note starts again (a rest or a tied note does not)
    bool new_step;  // a note or rest was read: the pitch is recalculated
    uint8_t note_offset;
    uint8_t instrument_number;
    MdlInstrument instrument;
    uint8_t random_wait_min;
    uint8_t random_wait_mask;
    uint8_t random_wait;
    uint8_t volume_table[MDL_VOLUME_STEPS];
    uint32_t call_stack[MDL_STACK_DEPTH];
    uint8_t call_depth;
    uint32_t loop_start[MDL_STACK_DEPTH];
    uint8_t loop_count[MDL_STACK_DEPTH];
    uint8_t loop_depth;
    // The pitch
    uint16_t base_period;
    uint16_t period;
    int16_t detune;
    uint8_t octave; // the note's octave (the note range for a sampled instrument)
    MdlWave wave;
    bool wave_built;
    uint16_t wave_phase;
    uint8_t period_shift;
    uint16_t wave_words;
    // The envelope and modulators
    uint16_t envelope;
    bool decaying;
    uint16_t volume;
    uint16_t window_offset;
    uint16_t phase_offset;
    uint8_t tone_offset;
    MdlModulatorState modulator[MDL_MODULATORS];
    int shown_note;
} MdlVoice;

struct MdlPlayer {
    MdlModule module;
    uint8_t* chip; // the silence and the wave buffers; AUDxLC of a voice can point here
    MdlSong song;
    bool playing;
    bool song_end; // a voice reached the end of its track in this tick
    bool ended;
    bool stalled; // a track step ran MDL_MAX_STEP_COMMANDS items (a module the proof accepted never does)
    uint16_t tick_count;
    uint16_t random_state;
    int8_t transpose;
    uint8_t speed;
    uint8_t arpeggio;
    uint8_t arpeggio_speed;
    uint8_t arpeggio_counter;
    uint8_t arpeggio_position;
    uint8_t volume_table[MDL_VOLUME_STEPS];
    MdlVoice voices[MDL_VOICES];
};

// The eagleplayer's Check2 (the "MSOB" mark) plus what this port needs: the four tables inside the file and a static
// proof that no subsong reads anything outside the file or the memory the port models (see mdl_coverage.c).
bool mdl_probe(const uint8_t* data, size_t size);

// Copies and parses the module; null when `mdl_probe` rejects it.
MdlPlayer* mdl_create(const uint8_t* data, size_t size);
void mdl_destroy(MdlPlayer* player);

int mdl_subsong_count(const MdlPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mdl_start(MdlPlayer* player, int subsong, NuadeVoices* voices);

MdlTickResult mdl_tick(MdlPlayer* player, NuadeVoices* voices);

MdlVoiceState mdl_voice_state(const MdlPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
MdlPlayer* mdl_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
