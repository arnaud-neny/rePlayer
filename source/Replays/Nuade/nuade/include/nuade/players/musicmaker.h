// SPDX-License-Identifier: MIT
// MusicMaker V8 player (Thomas Winischhofer's tracker), translated from the UADE eagleplayers "MusicMaker-4V" and
// "MusicMaker-8V". A song is a FORM "MMV8" file (an SDAT chunk with the song, an INST or PINS chunk with the
// instruments) or the older pair "name.sdata" + "name.i" / "name.ip". The 4 voice replayer drives Paula's channels
// directly; the 8 voice replayer mixes its voices into four buffers in software.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/player.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

// The 4 voice replayer programs its own CIA timer: its ticks are counted in cycles of the 709379 Hz CIA clock. The 8
// voice replayer is driven by the audio interrupt: its ticks are counted in colour clocks.
#define MM_TIMEBASE_HZ_4V 709379
#define MM_TIMEBASE_HZ_8V 3546895

#define MM4_VOICES 4
#define MM8_VOICES 8
#define MM_MAX_TRACKS 8
#define MM_PAIRS 4 // channel pairs of the 8 voice mixer (the 8 tracks share the 4 Paula channels)
#define MM8_CHANNELS MM_PAIRS
#define MM_MAX_PATTERNS 998 // the replayer's table of pattern pointers
#define MM_INSTRUMENTS 36   // entries of the instrument table
#define MM_PITCH_TABLES 15  // tables after the samples that the "set table" command picks from
#define MM_TABLE_SLOTS 37   // instrument slots a note can pick a table for

typedef struct MmPlayer MmPlayer;

typedef struct MmTickResult {
    uint32_t length; // until the next tick, in 1 / mm_timebase_hz seconds
    bool looped;     // the eagleplayer's SongEnd: every track has passed the end of its position list
} MmTickResult;

typedef struct MmVoiceState {
    int note;             // semitones from C-0 (12 = C-1) of the last note started, -1 if none
    int instrument;       // the instrument number of that note, -1 if none
    uint32_t note_starts; // notes that started a sample since the start
} MmVoiceState;

// What the replayer keeps for the hardware channel of a voice
typedef struct Mm4Channel {
    NuadePaulaChannel paula;
    uint32_t loop_sample; // the repeat the note start left for the DMA start
    uint16_t loop_words;
    bool started;    // a sample was started in this tick
    bool restarted;  // the voice's DMA was stopped for a new note in this tick
    bool dma_enable; // a note started in this tick: its DMACON bit is part of what the tick sets
} Mm4Channel;

// The software registers of a voice: what the sequencer leaves for the mixer.
typedef struct Mm8Voice {
    bool started;    // a sample was started in this tick
    uint32_t sample; // the sample it plays first (offset in the instrument block, 0 = none) and its length
    uint16_t length;
    uint32_t loop_sample; // the repeat (0 = none)
    uint16_t loop_length;
    uint16_t period;
    uint8_t volume;
    uint32_t data_ptr; // where the mixer left the voice after the last block
    uint16_t data_remaining;
    uint32_t resume_ptr; // the position the last 0xF1 event saw
    uint16_t resume_remaining;
} Mm8Voice;

// A voice's data in a channel's block: the mixer plays the voice with the lower number of a pair as B and the other as
// A.
typedef struct Mm8Stream {
    uint32_t ptr;
    uint16_t remaining;
} Mm8Stream;

typedef struct Mm8Channel {
    int8_t* buffer[2];    // the buffer that is mixed next, and the other one
    Mm8Stream stream[2];  // A, B
    uint16_t phase;       // the mixer's position inside the slower voice's samples (16 bit fraction)
    uint16_t last_period; // the channel period of the last block
    uint16_t carry;       // samples the last blocks were short or long by
    uint32_t accum;       // fraction of a sample the block lengths have left over (16.16)
    uint16_t pair_period[2];
    uint16_t pair_volume[2];
    // what the audio interrupt writes for the buffer that is mixed
    uint16_t period;
    uint16_t volume;
    uint16_t len_words;
    NuadePaulaChannel paula;
    uint64_t restart; // colour clock at which the buffer that plays now started: it runs out one buffer length later
} Mm8Channel;

typedef struct MmEffects {
    int16_t slide;        // added to the period every tick
    int16_t volume_slide; // added to the volume every tick
    int8_t mode;          // > 0: the volume slide changes sign every second tick, < 0: the period slide does
    bool toggle;          // which half of that pair the next tick is
    uint16_t glide_step;  // period change of a glide per 128 ticks
    uint16_t fade_step;   // volume change of a glide per 128 ticks
    uint16_t glide_base;  // period a glide starts from
    bool glide_down;      // the glide lowers the period
    bool fade_down;       // the glide lowers the volume
    uint8_t volume_base;  // volume a glide starts from
    uint8_t counter;      // ticks since the glide started (also runs without one)
} MmEffects;

typedef enum MmVariant {
    MmVariant_4V,
    MmVariant_8V,
} MmVariant;

typedef struct MmBlock {
    const uint8_t* data;
    uint32_t size;
} MmBlock;

typedef struct MmVoice {
    // the track
    const MmBlock* list; // the position list this track reads (an unused 8 voice track reads a list of its own)
    uint32_t list_pos;   // offset of the current entry
    uint32_t loop_pos;   // offset of the entry the list jumps back to
    uint32_t event;      // offset of the next event
    uint16_t countdown;  // ticks until it runs
    bool looped;         // the position list passed its end; the voice is cut at its next event
    bool ended;          // the voice has been cut after the list passed its end
    // the note
    uint16_t period;      // before the table's vibrato
    uint16_t note2;       // note index * 2
    uint16_t pitch_scale; // 0x100 = 1.0; bends move it
    uint16_t volume2;     // twice the volume
    uint32_t table;       // offset of the vibrato / tremolo table of the note, 0 = none
    uint16_t table_phase;
    uint16_t table_limit; // bytes the sample plays before its loop starts, as the phase counts them
    bool table_active;
    bool table_loop;
    uint16_t table_loop_length;
    MmEffects fx;
    bool bend_down;      // pitch bend towards 0x100 from above
    bool bend_up;        // pitch bend towards 0x2000
    bool volume_by_note; // the volume is divided by the note's divisor
    // what the last update left for the registers
    uint16_t out_period;
    uint8_t out_volume;
    // display
    int note;
    int instrument;
    uint32_t note_starts; // notes that started a sample since the start (the replayer keeps no count)
} MmVoice;

typedef struct MmSong {
    MmBlock block;
    uint8_t* storage; // the song's bytes (block points into it)
    char title[21];
    uint16_t tempo;                     // 4V: a tick is tempo * 23 CIA cycles; 8V: tempo * 0x73 colour clocks
    uint16_t pause_rows;                // rows a 0 entry of a position list waits
    bool has_track[MM_MAX_TRACKS];      // the tracks the song has
    bool pair_uses_020[MM_PAIRS];       // 8V: the channel pair is mixed by the 68020 routines, else by the 68000 ones
    uint16_t slow_limit;                // 8V: periods up to this are mixed at a slower channel period (68000 mixer)
    int32_t slow_table;                 // 8V: offset of the table of those channel periods (word per period)
    uint16_t start_index;               // first entry of every position list
    uint16_t loop_index;                // entry the lists jump back to
    uint32_t track_list[MM_MAX_TRACKS]; // offset of each position list
    uint32_t pattern[MM_MAX_PATTERNS];  // offset of the first event of each pattern
    uint32_t pattern_fallback;          // what an index past the table reads: the first pattern
} MmSong;

typedef struct MmSampleRecord {
    uint16_t length;       // bytes; 0 = no sample
    uint16_t first_length; // bytes played before the loop starts
    uint16_t loop_offset;  // bytes from the sample start to the loop
    uint16_t loop_length;  // words
} MmSampleRecord;

typedef struct MmInstruments {
    MmBlock block;
    uint8_t* storage;                // the block's bytes, then the Paula tail (nuade_paula_padded_copy)
    uint32_t sample[MM_INSTRUMENTS]; // offset of each sample
    MmSampleRecord record[MM_INSTRUMENTS];
    uint32_t pitch_table[MM_PITCH_TABLES]; // offset of each table, 0 = none
    uint32_t silent;                       // offset of the long before the first sample: the silent word
} MmInstruments;

typedef struct MmCore {
    MmVariant variant;
    const MmSong* song;
    const MmInstruments* inst;
    uint32_t voice_count;
    MmVoice voice[MM_MAX_TRACKS];
    uint32_t slot_table[MM_TABLE_SLOTS]; // offset of the table each instrument slot uses, 0 = none
    uint16_t tempo_base;                 // the tempo the table phase advances with
    uint8_t bend_down_step;
    uint8_t bend_up_step;
    bool tempo_requested; // a tempo command ran in this tick; the replayer applies new_tempo
    uint16_t new_tempo;
    bool song_end_reported;
} MmCore;

typedef struct Mm4 {
    MmCore core;
    Mm4Channel channel[MM4_VOICES];
    uint16_t timer;           // CIA cycles of one tick
    bool second_half_pending; // the tick started a sample: DMA comes on in a second interrupt
    bool first_tick;          // the timer has not fired yet
} Mm4;

typedef struct Mm8 {
    MmCore core;
    Mm8Voice voice[MM8_VOICES];
    Mm8Channel channel[MM8_CHANNELS];
    int8_t* memory;           // the buffers
    uint32_t buffer_size;     // bytes of one buffer
    uint32_t silent_capacity; // words of the silent block
    int8_t* tables;           // 64 rows of 256 bytes: row v maps a sample to v / 64 of it
    uint16_t tempo;           // the song's current tempo
    bool tempo_pending;
    uint16_t pending_tempo;
    uint64_t now; // colour clock of the current tick
} Mm8;

struct MmPlayer {
    MmVariant variant;
    MmSong song;
    MmInstruments instruments;
    Mm4 four;
    Mm8 eight;
};

// The eagleplayers' Check2: the FORM / song headers and, for the old "name.sdata", the file name.
bool mm_probe(const uint8_t* data, size_t size, const char* filename);

// 0 when the song is not accepted by mm_probe or an instrument file / memory is missing. The player keeps its own copy
// of what it needs.
MmPlayer* mm_create(const NuadeSongSource* source);
void mm_destroy(MmPlayer* player);

// Paula channels the replayer drives: 4 for both (the 8 voice song has 8 tracks mixed into them).
uint32_t mm_voice_count(const MmPlayer* player);
uint32_t mm_timebase_hz(const MmPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mm_start(MmPlayer* player, NuadeVoices* voices);

MmTickResult mm_tick(MmPlayer* player, NuadeVoices* voices);

// The song's tracks: 4, or 8 for the 8 voice replayer, which mixes two tracks into each Paula voice.
uint32_t mm_track_count(const MmPlayer* player);

// A track (0 .. mm_track_count - 1); with 4 tracks track n plays on Paula voice n.
MmVoiceState mm_voice_state(const MmPlayer* player, uint32_t track);

// Ticks one row of the display grid (NuadePlayer.grid_ticks) spans.
uint32_t mm_grid_ticks(const MmPlayer* player);

// The song title (the 20 bytes after the "SE" signature).
const char* mm_title(const MmPlayer* player);

// The player's state behind a song it opened; 0 when the song is another player's.
MmPlayer* mm_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
