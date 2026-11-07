// SPDX-License-Identifier: MIT
// Steve Barrett, translated from the UADE eagleplayer "SteveBarrett". The eagleplayer carries no replayer: every
// module is a 68k program (a game's sound driver) followed by IFF 8SVX samples, and the eagleplayer calls it through
// four `bra.w` entries. The driver exists in a few builds; this port models the builds listed in
// players/stevebarrett/sbr_module.c and refuses every other module, which UADE then plays.

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

#define SBR_VOICES 4

// The eagleplayer's file name prefix (the files are also named with it as a suffix).
#define SBR_EXTENSION "sb"

// The CIA clock: the timebase of the tick length.
#define SBR_TIMEBASE_HZ 709379

#define SBR_ENTRY_COUNT 4
#define SBR_MAX_INSTRUMENTS 32
// The bytes of the chord buffer for each voice: a start byte of 0, up to 8 chord notes, the $ff.
#define SBR_CHORD_BYTES 10u

typedef struct SbrPlayer SbrPlayer;

typedef enum SbrStatus {
    SbrStatus_Ok = 0,
    SbrStatus_InvalidData,
    SbrStatus_OutOfMemory,
} SbrStatus;

typedef struct SbrCreateResult {
    SbrPlayer* player;
    SbrStatus status;
} SbrCreateResult;

typedef struct SbrTickResult {
    uint32_t length; // this tick's length in 1 / SBR_TIMEBASE_HZ seconds
    bool looped;     // the eagleplayer called SongEnd: every voice has read the end of its pattern list
    bool ended;      // the song stopped itself
} SbrTickResult;

// What voice (0-based) last started a note: the period (0 if none yet) and the sample number.
typedef struct SbrVoiceState {
    uint32_t period;
    int instrument;
} SbrVoiceState;

typedef enum SbrLoopStyle {
    SbrLoop_EveryVoice, // the repeat of every voice is written after the DMA comes on; its length is in bytes
    SbrLoop_VoiceZero,  // only voice 0 gets a repeat, its length in words, and the wave record's two fields swap
} SbrLoopStyle;

typedef struct SbrBuild {
    const char* name;
    uint64_t code_hash;
    uint32_t code_start;
    uint32_t code_end;
    uint16_t entry[SBR_ENTRY_COUNT];
    bool extended;   // the chord commands and the note bend; otherwise the commands are notes
    bool reads_vhdr; // the init clears the headers and sets the wave table's loops from the VHDR chunks
    SbrLoopStyle loop_style;
    uint8_t max_song;      // the highest song number the song select entry takes
    uint64_t vars_hash;    // of the initial variables behind the code, the chord table left out
    uint32_t live_periods; // the four current periods, behind the period table
    uint32_t record;       // the voice records, further behind
    uint32_t period;
    uint32_t chord;
    uint32_t instruments; // the sample table the init builds: length in words, address
    uint32_t wave;
    uint32_t env;
    uint32_t songs;    // 8 bytes per song: the four lists
    uint32_t patterns; // 16-bit offsets
    uint32_t form;     // the first 8SVX file
} SbrBuild;

typedef struct SbrInstrument {
    uint16_t length_words;
    uint32_t offset;      // of the sample (the BODY chunk's data)
    uint32_t form_offset; // of its 8SVX file
} SbrInstrument;

typedef struct SbrModule {
    const SbrBuild* build;
    const uint8_t* bytes; // the module; the copy in `image` once the init has patched it
    size_t size;
    uint8_t* image; // owned: nuade_paula_padded_copy of the module, patched like the driver's init does
    uint32_t song_count;
    uint32_t tick_length; // in 1 / SBR_TIMEBASE_HZ seconds
    uint32_t instrument_count;
    SbrInstrument instruments[SBR_MAX_INSTRUMENTS];
} SbrModule;

// The chord of a voice: none, for this note only ($7d), or held for the next notes too ($7c).
typedef enum SbrChordMode {
    SbrChordMode_None,
    SbrChordMode_Note,
    SbrChordMode_Hold,
} SbrChordMode;

typedef struct SbrVoice {
    NuadePaulaChannel paula;
    uint8_t chord_base;           // $04: this voice's first byte in the chord buffer
    uint8_t note;                 // $08: note number with the list's transposition
    uint8_t list_pos;             // $0c: position in the pattern list
    uint8_t duration;             // $10: note length in steps
    uint8_t duration_left;        // $14
    SbrChordMode mode;            // $18
    uint8_t chord_mask;           // $1c: the notes of the chord
    uint8_t transpose;            // $20
    uint8_t bend_note;            // $34: the note before the one-tick bend; nonzero while the bend is up
    uint8_t chord_pos;            // $38: position in the chord buffer
    int8_t vibrato_dir;           // $3c
    uint8_t vibrato_step;         // $40
    uint8_t vibrato_delay_reload; // $44
    uint8_t vibrato_delay;        // $48
    uint8_t wave;                 // $4c: byte offset of the wave record
    uint8_t env;                  // $50: byte offset of the envelope record
    uint8_t volume_target;        // $54
    uint8_t volume_step;          // $58
    uint8_t volume_release;       // $5c
    uint8_t volume;               // $60
    uint8_t vibrato;              // $68: depth in the high nibble, speed in the low one
    bool released;                // $6c
    uint8_t sustain_shift;        // $70
    bool silent;                  // the voice's bit of the silent mask: its DMA is off
    uint16_t base_period;         // the period of the note, before vibrato and chords
    uint16_t period;              // the period written to Paula
    uint32_t sample;              // the module offset of the sample the note started
    uint16_t loop_length;         // of the wave record
    uint16_t loop_start;
    int32_t pattern; // the pattern's next byte, as an offset from a4
    int32_t list;    // the pattern list, the same
    int shown_instrument;
    uint16_t shown_period;
} SbrVoice;

struct SbrPlayer {
    SbrModule module;
    SbrVoice voices[SBR_VOICES];
    uint8_t chord_buffer[SBR_VOICES * SBR_CHORD_BYTES];
    uint8_t request;
    bool playing;
    uint8_t tempo;         // the number of ticks per step, less one
    uint8_t tick_count;    // counts the tempo down to 0 for the step
    uint8_t voice_counter; // the voice the tick works on; the eagleplayer's list-end hook reads it
    uint8_t last_pattern;  // the pattern number the last list read chose
    // High nibble of the vibrato depth: bits 8..11 of the period that a bend restore reloaded earlier in this tick; the
    // tick start, a note start and a release clear it.
    uint8_t vibrato_depth_high;
    bool stop_seen; // the stop routine has run once; from then on the eagleplayer calls SongEnd there
    bool list_pending[SBR_VOICES];
    bool looped;
    bool ended;
    bool broken;
};

// The eagleplayer's module check plus the proof that the module is a build the port models and that its songs stay
// inside what the port models.
bool sbr_probe(const uint8_t* data, size_t size);

SbrCreateResult sbr_create(const uint8_t* data, size_t size);
void sbr_destroy(SbrPlayer* player);

// The number of songs the eagleplayer's SubSongRange reports (at least 1).
uint32_t sbr_song_count(const SbrPlayer* player);

// (Re)start a song (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool sbr_start(SbrPlayer* player, uint32_t song, NuadeVoices* voices);

SbrTickResult sbr_tick(SbrPlayer* player, NuadeVoices* voices);

// The name of the build the module carries.
const char* sbr_format_name(const SbrPlayer* player);
SbrVoiceState sbr_voice_state(const SbrPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SbrPlayer* sbr_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
