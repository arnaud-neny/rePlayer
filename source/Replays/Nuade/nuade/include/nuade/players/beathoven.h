// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player, translated from the UADE eagleplayer "Beathoven Synthesizer" (Wanted Team). A module is
// an Amiga executable of one code hunk that holds the song, the samples and its own copy of the replayer
// ("BEATHOVEN100", "BEATHOVEN109" or "BEATHOVENNEW" after the first two instructions); the eagleplayer loads the hunk,
// patches a few instructions and calls the replayer's init and play routines. This port runs the replayer's logic on
// the module data.

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

#define BTH_VOICES 4

// The most subsongs a module is asked for
#define BTH_MAX_SUBSONGS 64

// The CIA clock: the timebase of the tick length.
#define BTH_TIMEBASE_HZ 709379

typedef struct BthPlayer BthPlayer;

typedef struct BthTickResult {
    bool looped; // every voice that plays has wrapped its track since the last time this was set (SongEnd)
} BthTickResult;

typedef struct BthVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // index of the instrument record of the last instrument command, -1 if none
} BthVoiceState;

typedef struct BthModule {
    uint8_t* data;
    uint32_t size;
} BthModule;

typedef struct BthImage {
    BthModule module;
    uint32_t* relocs; // sorted offsets of the relocated longwords
    uint32_t reloc_count;
    uint32_t version;      // the last four signature characters: "N100", "N109" or "NNEW"
    uint32_t load_address; // where UADE's loader puts the hunk; relocated pointers are the offset plus this
} BthImage;

// The header after the signature: 15 longwords
typedef struct BthHeader {
    uint32_t play; // the interrupt routine
    uint32_t init; // starts a subsong (the subsong number in d0)
    uint32_t subsong_count;
    uint32_t once;  // a routine the eagleplayer runs once when it loads the module; 0 for none
    uint32_t title; // the module information strings
    uint32_t author;
    uint32_t samples; // the instrument records: start and end
    uint32_t samples_end;
    uint32_t timer; // the play interrupt's CIA timer value, 0 for the default
} BthHeader;

typedef enum BthEngine {
    BthEngine_Basic,    // note, instrument, slide and hold commands
    BthEngine_Extended, // adds arpeggio and held-note commands
    BthEngine_Legacy,   // BEATHOVEN100: a replayer that drives the hardware through a helper routine
} BthEngine;

// A 16 bit value the replayer uses: an immediate in the code or a word of the module (the song init may write it)
typedef struct BthWord {
    bool in_module;
    uint16_t constant;
    uint32_t address;
} BthWord;

typedef struct BthReplayer {
    BthEngine engine;
    uint32_t voice_count;          // how many the play routine updates
    uint32_t play_flag;            // the play routine does nothing while this word is zero; 0 if it has none
    uint32_t last_voice_flag;      // the word that gates the last voice, 0 if it always runs
    bool last_voice_runs_when_set; // the last voice runs while the word is nonzero, else while it is zero
    BthWord tempo;                 // a note command's length is multiplied by this
    BthWord note_bias;             // added to the note number
    bool bias_in_record;           // the note offset is a field of the voice record
    bool bias_command;             // a command sets it
    uint32_t period_table;
    uint32_t sample_base; // added to an instrument's sample address, 0 for none (NEW)
    uint32_t silent;      // the address a voice that holds a note repeats
    bool slide_signed;    // the slide compares periods as signed numbers
    // BEATHOVEN100
    bool play_silences_channels;      // the play routine points every channel at the silent sample first
    int32_t note_offsets[BTH_VOICES]; // the play routine passes a voice its note offset
    bool uses_note_offset;            // the voice routine adds it to the note number
    uint32_t note_origin;             // subtracted from the note number: 1, the first period table entry
    uint32_t arpeggio_origin;         // the same in the arpeggio step
    bool index_unsigned;   // the code scales with mulu.w #2 (16 bit, unsigned) rather than a long shift: the period
                           // table index and the size of an arpeggio command both
    bool durations_scaled; // a length is scaled: (n / 4) * 5
} BthReplayer;

typedef struct BthTrackStart {
    uint32_t track; // the first entry of the voice's track list
    uint32_t loop;  // the entry the track list returns to at its end
} BthTrackStart;

// A word of the module the init routine sets
typedef struct BthWordWrite {
    uint32_t address;
    uint16_t value;
} BthWordWrite;

typedef struct BthSong {
    BthTrackStart voices[BTH_VOICES];
    BthWordWrite* writes;
    uint32_t write_count;
    uint32_t write_capacity;
} BthSong;

typedef struct BthVoice {
    NuadePaulaChannel paula;
    bool runs;                   // the play routine runs this voice
    bool was_running;            // the channel's DMA was on when the tick began
    uint32_t track;              // the current entry of the track list
    uint32_t loop;               // the entry the track list returns to
    uint16_t duration;           // ticks left of the note
    bool slide;                  // slide the period toward the note's
    bool legato;                 // a new note does not restart the sample
    bool one_shot;               // the instrument plays its sample once
    bool hold;                   // the note is held: the voice repeats the silent sample
    bool arpeggio;               // cycle through the arpeggio table
    uint32_t instrument_address; // BEATHOVEN100: the instrument record
    uint32_t command;            // the next command of the stream
    uint32_t note_base;          // the track entry's note offset
    uint16_t target_period;
    uint16_t period;
    uint16_t slide_step;
    uint16_t length_words; // of the sample the instrument set
    uint32_t sample;
    int16_t loop_start; // offsets into the sample
    int16_t loop_end;
    uint16_t one_shot_length; // the instrument's played-once length, bytes
    uint16_t arpeggio_count;
    uint32_t arpeggio_table;
    uint16_t arpeggio_index;
    uint16_t note_bias; // the note offset of a record that has one
    bool wrapped;       // the track list returned to its loop entry since the last SongEnd
    int note;
    int instrument;
} BthVoice;

struct BthPlayer {
    BthImage* image;
    BthModule work; // the module with the init routine's writes: the replayer's variables
    BthHeader header;
    BthReplayer replayer;
    BthSong* songs;
    uint32_t song_count;
    uint32_t tick_length;
    char title[128]; // the module's information strings
    char author[128];
    BthVoice voices[BTH_VOICES];
};

// Whether bth_create opens the file: the eagleplayer's Check2 (a hunk executable with the memory flags byte set, and
// the replayer signature), and a replayer and songs this port knows.
bool bth_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer) and finds the replayer's routines in it; null if it is not a
// module or its replayer is not one this port knows.
BthPlayer* bth_create(const uint8_t* data, size_t size);
void bth_destroy(BthPlayer* player);

uint32_t bth_subsong_count(const BthPlayer* player);

// The module's information strings
const char* bth_title(const BthPlayer* player);
const char* bth_author(const BthPlayer* player);

// The play interrupt's period in 1 / BTH_TIMEBASE_HZ seconds.
uint32_t bth_tick_length(const BthPlayer* player);

// The ticks a note length of the subsong is a multiple of
uint32_t bth_ticks_per_step(const BthPlayer* player, uint32_t subsong);

// Start a subsong from its beginning (the eagleplayer's InitSound); the player drives `voices` (Paula channel n =
// voice n).
bool bth_start(BthPlayer* player, uint32_t subsong, NuadeVoices* voices);

BthTickResult bth_tick(BthPlayer* player, NuadeVoices* voices);

BthVoiceState bth_voice_state(const BthPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
BthPlayer* bth_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
