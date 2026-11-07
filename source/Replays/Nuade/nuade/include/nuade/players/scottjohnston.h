// SPDX-License-Identifier: MIT
// Scott Johnston's SoundPlayer, translated from the UADE eagleplayers "ScottJohnston" and "SoundPlayer". The
// eagleplayers carry the replayer; a song is data: a tune of 12-byte rows that four tracks walk on their own. A
// ScottJohnston song ("sj.<name>") has a header with the instrument files to load, IFF 8SVX files in the "instruments"
// directory next to the song; a SoundPlayer song ("sjs.<name>") is the tune alone and its instruments are the 8SVX
// forms of the file "smp.<name>". The module formats and the replayer are described in players/scottjohnston/.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/io.h>
#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SJ_VOICES 4

// The eagleplayers' file name prefixes (the ScottJohnston one also as a suffix).
#define SJ_EXTENSION "sj"
#define SJS_EXTENSION "sjs"

// The CIA clock: the timebase of the tick length.
#define SJ_TIMEBASE_HZ 709379

#define SJ_FILE_SLOTS 16
// The replayer's tables.
#define SJ_INSTRUMENT_SLOTS 38

typedef struct SjPlayer SjPlayer;

typedef enum SjStatus {
    SjStatus_Ok = 0,
    SjStatus_InvalidData,
    SjStatus_MissingInstruments, // an instrument file of the header could not be read
    SjStatus_OutOfMemory,
} SjStatus;

typedef struct SjCreateResult {
    SjPlayer* player;
    SjStatus status;
} SjCreateResult;

typedef struct SjTickResult {
    uint32_t length; // this tick's length in 1 / SJ_TIMEBASE_HZ seconds
    bool looped;     // the replayer called SongEnd (SoundPlayer: the restart of a track)
    bool ended;      // the song cannot continue (crafted data)
} SjTickResult;

// What voice (0-based) last started: the period (0 if none yet) and the instrument number.
typedef struct SjVoiceState {
    uint32_t period;
    int instrument;
} SjVoiceState;

// One voice's part of a row.
typedef struct SjCell {
    uint8_t note;
    uint8_t instrument;
    uint8_t command;
} SjCell;

typedef struct SjVoice {
    NuadePaulaChannel paula;
    uint32_t sample_start;        // the note to start: the instrument's body (arena offset)
    uint16_t sample_length_words; // 0 = nothing to start
    uint16_t period;
    uint16_t volume;
    uint32_t loop_start; // the repeat registers the voice rewrites every tick
    uint16_t loop_length_words;
    SjCell cell; // the row's cell, until its note has started and its command run
    bool hold;   // the repeat registers are left alone
    bool fade_down;
    uint8_t fade_speed; // 0 = no fade
    uint8_t fade_counter;
    uint32_t shown_period;
    int shown_instrument;
} SjVoice;

// The two eagleplayers that run this replayer, and where they differ.
typedef enum SjFormat {
    SjFormat_ScottJohnston, // song "sj.<name>", instrument files named in the header
    SjFormat_SoundPlayer,   // song "sjs.<name>", instruments in the sample bundle "smp.<name>"
} SjFormat;

typedef struct SjVariant {
    SjFormat format;
    const char* name;
    uint32_t tune_offset; // the tune's own header in the module
    bool init_runs_tick;  // the init runs one interrupt (the row countdown stands one lower); else it stands at 0
    bool init_dma_on;     // the init leaves the DMA of all voices on (else a voice's DMA comes on with its first note)
    bool arms_all_voices; // every voice walks a track, whatever the tune's mask says
    bool stop_hits_own_voice; // "stop channel" switches off the voice's own DMA (else voice 0 hits voice 1)
    bool restart_ends_song;   // the track restart calls the eagleplayer's SongEnd
} SjVariant;

typedef struct SjModule {
    const SjVariant* variant;
    const uint8_t* data;
    size_t size;
    uint16_t timer;                    // the CIA timer of the tick
    uint8_t voice_mask;                // bit n: voice n walks a track; the others stay silent
    uint8_t file_index[SJ_FILE_SLOTS]; // instrument file names, 1-based into the name table; 0 = repeat the last
} SjModule;

// One track of the tune; the voice of the same number reads its cell from the track's row.
typedef struct SjTrack {
    int32_t row;        // the row the track stands at; -1 before the first
    int32_t loop_row;   // the row of the last loop begin
    uint8_t loop_count; // repeats left, SJ_NO_LOOP outside a loop
    uint8_t wait;       // rows still to hold the current row
} SjTrack;

// The replayer's record of an instrument file: where the 8SVX body is and its two parts, in the sample arena.
typedef struct SjSampleSlot {
    uint32_t start;
    uint16_t length_words;
    uint32_t loop_start;
    uint16_t loop_length_words;
} SjSampleSlot;

typedef struct SjSamples {
    uint8_t* arena; // the files back to back behind a silent word, padded for Paula
    size_t arena_size;
    SjSampleSlot slots[SJ_INSTRUMENT_SLOTS];
} SjSamples;

struct SjPlayer {
    SjModule module;
    SjSamples samples;
    SjTrack tracks[SJ_VOICES];
    SjVoice voices[SJ_VOICES];
    uint8_t row_countdown;
    uint32_t tick_length;
    bool broken; // a track left the data that the proof covers (crafted data)
    bool looped; // this tick called the eagleplayer's SongEnd
};

// The song's file name carries the eagleplayer's prefix ("sj": the eagleplayer has no module check of its own; "sjs":
// its module check is part of the probe), and the header and the tune are what `sj_create` can follow.
bool sj_probe(const uint8_t* data, size_t size, const char* filename);

// The eagleplayer's InitPlayer: parses the header, loads the instruments, and proves that the tune stays inside the
// data the port models.
SjCreateResult sj_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void sj_destroy(SjPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool sj_start(SjPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices. Only the SoundPlayer replayer calls SongEnd.
SjTickResult sj_tick(SjPlayer* player, NuadeVoices* voices);

// "Scott Johnston" or "SoundPlayer": the eagleplayer the song belongs to.
const char* sj_format_name(const SjPlayer* player);

// Interrupts one row of the tune spans.
uint32_t sj_ticks_per_row(void);
SjVoiceState sj_voice_state(const SjPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SjPlayer* sj_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
