// SPDX-License-Identifier: MIT
// Anders 0land player (HOT, 1993), translated from the UADE eagleplayer "Anders 0land player module" (Wanted Team).
// The module format and the replayer are described in players/andersoland/andersoland.c.

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

#define AO_VOICES 4

// The CIA clock: the timebase of AO_TICK_LENGTH.
#define AO_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / AO_TIMEBASE_HZ seconds.
#define AO_TICK_LENGTH 14187

typedef struct AoPlayer AoPlayer;

typedef struct AoTickResult {
    bool looped; // every voice reached the end of its track list during this tick (the eagleplayer's SongEnd)
    bool ended;  // the same, and every voice has stopped: the song cannot continue
} AoTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct AoVoiceState {
    int note;
    int instrument;
} AoVoiceState;

typedef struct AoModule {
    uint8_t* data; // nuade_paula_padded_copy of `size` bytes: the file, then zeros up to the last sample start
    uint32_t size;
    uint32_t samples; // the msm chunk's data: sample table entries are offsets from here
    uint32_t track_lists;
    uint32_t loop_points;
    uint32_t subsong_params;
    uint32_t patterns;
    uint32_t instruments;
    uint32_t envelopes;
    uint32_t effects;
    uint32_t sample_starts;
    uint32_t sample_lengths;
    uint32_t loop_starts;
    uint32_t loop_lengths;
    uint32_t subsong_count;
} AoModule;

typedef struct AoVoice {
    NuadePaulaChannel paula;

    // Sequencer
    bool enabled;         // reads events (a stopped voice stays off)
    bool dma;             // the replayer switches the voice's DMA on
    bool new_note;        // read an event in the current step
    bool prev_new_note;   // the same for the previous step
    bool legato;          // the event's note keeps the sample running
    bool end_pending;     // SongEnd bookkeeping: has not reached the end of its track list since the last SongEnd
    uint32_t track;       // the voice's track list
    uint8_t track_pos;    // next entry of the track list
    uint8_t track_loop;   // where the list continues after TRACK_LOOP
    bool has_pattern;     // false until the first pattern: the replayer starts on a pattern that is just PATTERN_END
    uint32_t pattern;     // the playing pattern
    uint8_t pattern_pos;  // next event of the pattern
    uint8_t transpose;    // semitones added to every note
    uint8_t duration;     // steps left before the next event
    uint8_t instrument;   // the last instrument named by an event
    uint8_t sample;       // sample number of the last event
    uint8_t prev_sample;  // `sample` as the previous step left it
    bool alternate_taken; // an alternate-flag event has loaded the second envelope and sample
    uint16_t note_period; // period of the last note byte
    uint16_t slide_next;  // pitch slide the last event asked for, taken at the step end
    int note;             // display only

    // Sound
    uint16_t period; // the base period, changed by the slide each tick
    uint16_t slide;
    uint8_t envelope; // index of the envelope entry that follows the one running
    uint8_t env_step;
    uint8_t env_count;
    uint8_t volume;
    uint8_t vibrato_delay;
    int8_t vibrato_rate;
    uint8_t vibrato_period;
    uint8_t vibrato_count;
    uint16_t vibrato_phase;
    int8_t vibrato_offset; // added to the period
} AoVoice;

typedef struct AoSong {
    uint8_t speed; // ticks per step
    uint8_t tick;  // tick within the step
    bool fade;
    uint16_t fade_count;
    bool looped; // SongEnd was called with voices still playing
    bool ended;  // SongEnd was called and every voice has stopped
    AoVoice voices[AO_VOICES];
} AoSong;

struct AoPlayer {
    AoModule module;
    AoSong song;
};

// The eagleplayer's check: the three chunk tags and sizes.
bool ao_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer); null if it is not a valid module.
AoPlayer* ao_create(const uint8_t* data, size_t size);
void ao_destroy(AoPlayer* player);

int ao_subsong_count(const AoPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ao_start(AoPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
AoTickResult ao_tick(AoPlayer* player, NuadeVoices* voices);

// Ticks one sequencer step spans (the subsong's speed byte); 0 for a bad subsong.
uint32_t ao_grid_ticks(const AoPlayer* player, int subsong);
AoVoiceState ao_voice_state(const AoPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
AoPlayer* ao_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
