// SPDX-License-Identifier: MIT
// Ashley Hogg game sound drivers, translated from the UADE eagleplayer "Ashley Hogg" (Wanted Team).
// The module formats and the replayers are described in players/ashleyhogg/.

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

#define AH_VOICES 4

// The CIA clock: the timebase of AH_TICK_LENGTH.
#define AH_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / AH_TIMEBASE_HZ seconds.
#define AH_TICK_LENGTH 14187

typedef struct AhPlayer AhPlayer;

typedef enum AhStatus {
    AhStatus_Ok = 0,
    AhStatus_InvalidData,
    AhStatus_MissingSamples, // the "smp." file next to the song could not be read
    AhStatus_OutOfMemory,
} AhStatus;

typedef struct AhCreateResult {
    AhPlayer* player;
    AhStatus status;
} AhCreateResult;

typedef struct AhTickResult {
    bool looped; // the driver reached the point where the eagleplayer calls SongEnd
    bool ended;  // the driver has silenced every voice
} AhTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct AhVoiceState {
    int note;
    int instrument;
} AhVoiceState;

typedef struct AhMemory {
    uint8_t* data;
    uint32_t size;     // of the file
    uint32_t capacity; // zero after `size`; the Paula tail follows
} AhMemory;

// The tables the code addresses; all are offsets into the module.
typedef struct AhOldTables {
    uint32_t periods;         // words, indexed by note
    uint32_t instruments;     // OLD_INSTRUMENT_SIZE bytes each
    uint32_t arp_offsets;     // longs: offset of each arpeggio from `arp_base`
    uint32_t arp_base;        // signed bytes
    uint32_t samples;         // sample offsets of the instruments are relative to this
    uint32_t silent;          // the long here is set to hold the address of the silent word that follows it
    uint32_t track_base;      // track offsets are relative to this; a voice whose track is `track_base` has none
    uint32_t pattern_offsets; // longs: offset of each pattern from `pattern_base`
    uint32_t pattern_base;
    uint32_t speeds;    // a byte per subsong: ticks per step
    uint32_t songs;     // OLD_SONG_ENTRY_SIZE bytes per subsong: the track offsets of the four voices
    uint32_t wave_base; // the waveforms of the wave-table voice
    uint32_t subsong_count;
} AhOldTables;

// What differs between the versions of the driver, told apart by instructions of the code.
typedef struct AhOldVersion {
    bool music_on_all_voices; // else only voice 3 plays the song (the others are for the game's sound effects)
    bool restart_offset;      // the track restart command adds the byte that follows it
    uint8_t fade_command;     // track command that starts a fade out (its byte is the ticks per volume step), 0 = none
} AhOldVersion;

// The instrument record the driver copies into the voice when a note names an instrument, and changes while it plays.
typedef struct AhOldInstrument {
    uint8_t attack_rate;   // ticks per volume step up, 0 = jump to `sustain_level`
    uint8_t sustain_level; // volume to reach and hold; 0 = release
    uint8_t release_rate;  // ticks per volume step down, 0 = silent at once
    uint8_t level;         // the volume
    uint16_t arp_table;    // which arpeggio
    uint16_t arp_pos;      // position in it
    uint16_t length_ticks; // ticks until the note is released, 0 = never
    uint16_t wave_pos;     // wave-table voice: position, wraps in 12 bits
    int16_t wave_step;     // wave-table voice
    uint8_t wave_limit_lo; // the step reverses when the high byte of `wave_pos` is one of the limits
    uint8_t wave_limit_hi;
    int16_t slide;          // pitch change per tick
    uint8_t vibrato_count;  // ticks left of the half period
    uint8_t vibrato_period; // ticks per half period
    int16_t vibrato_step;
    uint16_t vibrato_delay;
    int16_t vibrato_growth;
    int16_t vibrato_max;
    bool wave_table;  // the voice plays the wave table, not a sample
    bool dma_started; // the sample's DMA has been switched on (a record with sample offset 0 never starts)
    uint32_t sample;  // offset of the sample from the sample base
    uint32_t loop;    // offset of the repeat, 0 = silent repeat
    uint16_t length_words;
    bool transpose_note; // the transposition is added to the note
} AhOldInstrument;

typedef struct AhOldVoice {
    AhOldInstrument instrument;
    int16_t pitch;           // slide and vibrato, added to the period
    uint8_t step_count;      // ticks until the next pattern event
    uint8_t step_ticks;      // ticks per step
    uint8_t wait;            // steps to wait before the next pattern event
    uint8_t pattern_repeats; // repeats of the pattern left
    uint32_t track;          // next track entry
    uint32_t pattern;        // next pattern byte
    uint32_t track_restart;
    uint32_t pattern_start; // where the pattern repeats from
    int16_t note;
    int16_t transposition;
    int16_t transpose_base; // from the track entry
    uint8_t env_count;      // ticks until the next envelope step
    bool alive;             // SongEnd bookkeeping: still has to reach its restart command
    bool alive_initial;     // `alive` after a SongEnd
    int display_note;
    int display_instrument;
} AhOldVoice;

typedef struct AhOldSong {
    AhOldVoice voices[AH_VOICES];
    NuadePaulaChannel paula[AH_VOICES];
    uint16_t master;     // volume of all voices, 0 = the driver is stopped
    uint16_t fade_count; // ticks until the next fade step
    uint16_t fade_ticks; // ticks per fade step, 0 = no fade
    bool started;        // the first tick has loaded the tracks
    bool track_loaded;   // a voice has loaded a track entry in this tick
    uint8_t speed;       // ticks per step of the subsong
    bool looped;
    bool ended;
} AhOldSong;

typedef struct AhOld {
    AhOldTables tables;
    AhOldVersion version;
    AhOldSong song;
} AhOld;

// The tables the code addresses; all are offsets into the module.
typedef struct AhNewTables {
    uint32_t songs;     // per subsong the offsets of its order list and of its pattern table, from this table
    uint32_t periods;   // rows of NEW_PERIOD_ROW_SIZE bytes
    uint32_t remap;     // per subsong 16 bytes: the sample number of each instrument number
    uint32_t transpose; // a word per subsong, added to the note
    uint32_t durations; // bytes: rows of a note by its two duration bits
    uint32_t subsong_count;
} AhNewTables;

// A record of the sample file.
typedef struct AhNewSample {
    uint32_t start; // offsets in the file
    uint32_t loop_start;
    uint16_t start_length; // words
    uint16_t loop_length;
    uint8_t volume;
    uint16_t period_row; // which row of the period table the sample uses
} AhNewSample;

typedef enum AhNewEffect {
    AhNewEffect_None = 0,
    AhNewEffect_SlideUp,   // period offset decreases by the parameter
    AhNewEffect_SlideDown, // period offset increases
    AhNewEffect_Nothing,
    AhNewEffect_VolumeSlide,
    AhNewEffect_VolumeSlide2, // the same routine
    AhNewEffect_SetVolume,
} AhNewEffect;

typedef struct AhNewVoice {
    uint32_t seq;          // next event byte, 0 = none
    uint32_t loop_return;  // where a loop continues when its count is used up
    uint8_t instrument;    // from the last instrument command
    bool repeating;        // a loop is counting
    uint16_t repeat_count; // bytes left in the loop
    uint8_t note;          // 1.. = index of the period, 0 = none yet
    uint8_t duration;      // steps before the next event
    bool volume_locked;    // the sample's volume is not loaded when the note starts
    uint8_t volume;
    uint16_t pending_sample; // sample number + 1 of a note that has to start, 0 = none
    AhNewSample sample;      // of the last note with an instrument
    AhNewEffect effect;
    uint8_t effect_ticks; // steps left of the changing effect parameter; the length is built from bits of two bytes
    int8_t effect_step;
    uint16_t effect_param;
    int16_t period_offset;
    bool effect_new; // the effect starts on the next tick
    bool retrigger;  // the note loads the sample's volume even if an effect set the volume
    int display_note;
    int display_instrument;
} AhNewVoice;

typedef struct AhNewSong {
    AhNewVoice voices[AH_VOICES];
    NuadePaulaChannel paula[AH_VOICES];
    int subsong;         // 0-based
    int pending_song;    // number (1-based) of a song to start on the next tick, -1 = none
    uint32_t order_list; // the order list's length byte
    uint32_t pattern_table;
    uint16_t order_pos;
    uint16_t order_len;
    uint16_t phase;       // tick of the step
    uint16_t phase_limit; // ticks per step
    bool looped;
} AhNewSong;

typedef struct AhNew {
    AhNewTables tables;
    AhNewSong song;
    AhMemory samples; // the smp file
} AhNew;

typedef enum AhFamily {
    AhFamily_Old,
    AhFamily_New,
} AhFamily;

struct AhPlayer {
    AhFamily family;
    AhMemory module;
    AhOld old_family;
    AhNew new_family;
};

// The eagleplayer's check: a table of branches at the start, then the driver's first instructions.
bool ah_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer) and loads the "smp." sample file that the newer drivers keep
// next to the song.
AhCreateResult ah_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void ah_destroy(AhPlayer* player);

int ah_subsong_count(const AhPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool ah_start(AhPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
AhTickResult ah_tick(AhPlayer* player, NuadeVoices* voices);

// Ticks one sequencer step spans (the song's speed); 0 for a bad subsong.
uint32_t ah_grid_ticks(const AhPlayer* player, int subsong);
AhVoiceState ah_voice_state(const AhPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
AhPlayer* ah_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
