// SPDX-License-Identifier: MIT
// Jesper Olsen player (Amiga game music, 1990-95: Lollypop, Georg Glaxo, Harald Hardtand, Guldkornsexpressen),
// translated from the UADE eagleplayers "Jesper Olsen" (Wanted Team) and "Jesper Olsen EP" (Wombat & Defect).
//
// Jesper Olsen wrote a new replayer for each game; the eagleplayers know four of them. Two of them live in the
// module (68k code at the start of the file), two are data-only modules that the eagleplayer carries the replayer
// for. The player tells them apart by the module and runs the replayer of that version.

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

#define JO_VOICES 4

// The CIA clock: the timebase of JO_TICK_LENGTH.
#define JO_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / JO_TIMEBASE_HZ seconds.
#define JO_TICK_LENGTH 14187

#define JO_C_RECORDS 8

typedef struct JoPlayer JoPlayer;

typedef struct JoTickResult {
    bool looped; // every voice passed its loop point or its end since the last time they all had
    bool ended;  // and no voice plays any more
} JoTickResult;

typedef struct JoVoiceState {
    int note;       // the note of the voice's last row, -1 if none yet
    int instrument; // instrument the voice plays
} JoVoiceState;

typedef struct JoAVersion JoAVersion;

typedef enum JoReplayer {
    JoReplayer_A,
    JoReplayer_B,
    JoReplayer_C,
} JoReplayer;

typedef enum JoKind {
    JoKind_A1,
    JoKind_A2,
} JoKind;

// Where a version keeps its tables in the module, and how it differs
struct JoAVersion {
    JoKind kind;
    uint32_t signature; // FNV-1a of the module bytes before the period table
    uint32_t periods;
    uint32_t sample_entries; // A1: the table that maps a sample number to its record offset
    uint32_t samples;        // the sample records are addressed from here (A2: from before the module start)
    uint32_t silence;
    uint32_t songs;
    uint32_t song_size;
    uint32_t patterns;
    uint16_t first_entry; // the sample entry of the first sample record
    uint8_t level_mask;   // the sample's volume level survives under this mask
    int16_t counter_bias; // added to a row length to give the voice counter
    bool signed_offsets;  // the jump target and the pattern offset of a track entry are added as signed words
};

typedef struct JoModule {
    const uint8_t* data;
    uint32_t size;
} JoModule;

// The eagleplayer's song end: one flag per voice that had a track when the song started, cleared when the voice
// passes the loop point of its track or reaches its end. When the last flag clears, the replayer calls SongEnd and the
// flags are set again.
typedef struct JoSongEnd {
    bool initial[JO_VOICES];
    bool pending[JO_VOICES];
    bool all_passed; // set during the tick; cleared by jo_song_end_result
} JoSongEnd;

typedef struct JoCProgram {
    uint16_t position; // offset of the next (field, value) pair; 0 = no program
    int8_t counter;
    uint8_t reload;
} JoCProgram;

// The module's voice record (0x44 bytes): everything the replayer keeps per voice
typedef struct JoCVoice {
    uint16_t instrument;  // low byte indexes the instrument table
    uint8_t speed;        // added to the tempo accumulator every tick
    uint8_t row_duration; // the last special row's duration; 0xff once a note has started
    int8_t repeat;        // pattern repeat counter
    bool fixed_pitch;     // the base note is the note
    uint8_t base_note;
    uint8_t transpose;
    int8_t slide;
    int8_t pitch_a;
    int8_t pitch_b;
    uint8_t volume_c;
    uint8_t volume_b;
    uint8_t volume_a;
    bool no_pitch_a; // pitch_a is ignored
    bool tie;        // the next trigger of a tie instrument keeps the sound
    uint8_t wave;    // wave number of a multi wave instrument
    uint16_t slide_sum;
    JoCProgram programs[3];
    uint32_t instrument_table; // offset of the word table that locates the instrument records
    uint8_t note;              // JoNote_Silent: no sound
    uint8_t duration;          // ticks left in the row (bit 7 of the row's length is dropped)
    uint16_t track_position;   // offset of the next track entry; 0 = the voice is not playing
    uint16_t pattern_position;
    uint8_t channel;
    uint8_t priority;
    uint8_t tempo; // accumulator: the row counts down when it carries
    bool new_note;
    bool started; // a row with a note ran (for the display)
} JoCVoice;

typedef struct JoC {
    JoCVoice records[JO_C_RECORDS];
    uint32_t record_offsets[JO_C_RECORDS];
    int record_count;
    JoCVoice* slots[JO_VOICES]; // the voice record each Paula channel plays
    uint8_t master;
    bool dma_off_next[JO_VOICES]; // channels cut at the start of the next tick
    bool dma_on[JO_VOICES];       // channels started at the end of this tick
    NuadePaulaChannel paula[JO_VOICES];
    JoSongEnd end;
} JoC;

// The module's voice record: everything the replayer keeps per voice
typedef struct JoBVoice {
    uint16_t instrument; // offset of the instrument record
    uint8_t note;        // JoNote_Silent: no sound
    uint8_t counter;     // ticks left in the row
    uint16_t track_position;
    bool step_now; // bit 15 of the track position: the row ends at every tick, not only at tempo steps
    uint16_t pattern_position;
    uint16_t sample_position; // the three programs of the instrument: sample blocks, pitch and volume
    uint16_t pitch_position;
    uint16_t volume_position;
    uint16_t slide_sum;
    bool portamento_no_wait; // the portamento program runs without its wait
    bool portamento_off;     // the portamento program does not run
    uint8_t slide_speed;
    uint16_t volume;
    uint8_t row_duration; // 0xff once a note has started
    uint8_t note_offset;
    int8_t repeat;
    uint16_t master;
    uint8_t tempo_command; // 0x86: the voice sets the global tempo every tick
    uint8_t tempo_value;
    uint16_t portamento_position;
    bool pitch_no_portamento; // the pitch program drops the portamento of the tick
    bool pitch_add_slide;     // and adds the slide sum
    bool pitch_absolute;      // its note is not relative to the row's note
    bool pitch_hold;          // the program stays on this entry while a note plays
    uint8_t pitch_note;
    int8_t portamento_count;
    int8_t portamento_value;
    uint16_t volume_level;
    int8_t pitch_counter;
    int8_t portamento_wait;
    int8_t volume_counter;
    int8_t sample_counter;
    uint16_t detune;
    uint8_t channel;
    bool started;
} JoBVoice;

typedef struct JoB {
    JoBVoice voices[JO_VOICES]; // in the order of the song record; voice n plays on channel 3 - n
    uint8_t tempo;              // two tick counts, low nibble then high nibble, between sequencer steps
    int8_t counter;
    bool high_nibble;
    NuadePaulaChannel paula[JO_VOICES];
    JoSongEnd end;
} JoB;

// The programs of an A voice, the index into its positions, offsets and counters
enum {
    JoProgram_Sample,
    JoProgram_Note,
    JoProgram_Volume,
    JoProgram_Pitch,
    JoProgram_Count,
};

typedef struct JoAVoice {
    int16_t counter;       // ticks left in the row
    uint16_t reload;       // the length of the next row (0x4000 and up: the next note keeps its sample)
    uint16_t sample_entry; // locates the sample record
    uint16_t track_position;
    uint16_t pattern_position;
    uint16_t step;                      // ticks since the note started
    uint16_t programs[JoProgram_Count]; // positions of the sample, note, volume and pitch programs
    uint16_t offsets[JoProgram_Pitch];  // A1: added to the sample, note and volume program positions when a note starts
    int8_t counters[JoProgram_Count];   // ticks to the next step of each program
    bool keep_slide_speed;              // the next row keeps the slide speed
    bool keep_slide_sum;                // and the slide sum
    bool no_pitch_program;
    uint8_t portamento; // A1: target note (negative: none); A2: offset to the target note (0: none)
    uint16_t slide_position;
    uint16_t slide_speed;
    uint16_t last_period;
    bool pitch_ignores_step; // the pitch program starts at once
    bool pitch_off;          // there is no pitch program
    uint16_t table_base;     // A1: added to the sample numbers of a pattern
    uint8_t instrument_volume;
    uint8_t volume_level;
    uint8_t note;
    uint8_t played_note;
    int8_t repeat; // A1
    uint8_t transpose;
    bool rest;
    bool started;
} JoAVoice;

typedef struct JoA {
    const JoAVersion* version;
    JoAVoice voices[JO_VOICES];
    uint8_t master_high; // A1: set by a track command
    uint8_t master;
    uint8_t transpose; // A2: set by a track command
    uint8_t tempo[2];  // two tick counts between tempo steps, taken in turn
    int8_t counter;
    bool second_tempo;
    bool active[JO_VOICES];  // the voices that are playing
    bool dma_on[JO_VOICES];  // channels whose note started this tick
    bool dma_off[JO_VOICES]; // channels whose row ends: stopped at the next tempo step
    NuadePaulaChannel paula[JO_VOICES];
    JoSongEnd end;
} JoA;

struct JoPlayer {
    uint8_t* storage; // nuade_paula_padded_copy of the module
    JoModule module;
    JoReplayer replayer;
    const char* replayer_name;
    const JoAVersion* version_a;
    union {
        JoA a;
        JoB b;
        JoC c;
    } state;
};

// The eagleplayers' module check: a data module (a table of song offsets, each list behind a 0x7fff word, or the
// header of the Harald Hardtand games), or a module that starts with the replayer's own code of a known version.
bool jo_probe(const uint8_t* data, size_t size);

// Copies the module; 0 when it is not one of the known formats.
JoPlayer* jo_create(const uint8_t* data, size_t size);
void jo_destroy(JoPlayer* player);

int jo_subsong_count(const JoPlayer* player);
// The replayer version found ("replayer C", ...), for the metadata.
const char* jo_replayer_name(const JoPlayer* player);

// (Re)start a subsong (0-based) from its beginning.
bool jo_start(JoPlayer* player, int subsong);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
JoTickResult jo_tick(JoPlayer* player, NuadeVoices* voices);

JoVoiceState jo_voice_state(const JoPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
JoPlayer* jo_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
