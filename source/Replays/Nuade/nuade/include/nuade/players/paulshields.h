// SPDX-License-Identifier: MIT
// Paul Shields player (the music driver of Paul Shields' games: Gold Runner II, Air Ball, Amnios, Amiga Soccer, ...),
// translated from the UADE eagleplayer "PaulShields". The replayer is in the eagleplayer; a module holds a sample
// header, the song data (order lists, patterns, envelopes) and the sample bytes. Three module layouts exist (told apart
// by the eagleplayer's Check2): two share one sequencer, the oldest has its own.

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

#define PSH_VOICES 4

// The CIA clock: the timebase of PSH_TICK_LENGTH.
#define PSH_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / PSH_TIMEBASE_HZ seconds.
#define PSH_TICK_LENGTH 14187

// The sample table holds the 16 records of the module; the layouts with envelope tables continue it with the replayer's
// own wave forms.
#define PSH_SAMPLE_COUNT 16
#define PSH_WAVE_COUNT 7
#define PSH_ORDER_CODE_COUNT 6

typedef struct PshPlayer PshPlayer;

typedef struct PshTickResult {
    bool looped; // the eagleplayer's SongEnd call: an order list reached its loop or stop mark
    bool ended;  // every voice has stopped
} PshTickResult;

typedef struct PshVoiceState {
    int note;       // semitones from C-0 of the voice's last note, -1 if none yet
    int instrument; // sample (or instrument) number the voice plays, -1 if none yet
} PshVoiceState;

// The three module layouts, told apart by the eagleplayer's Check2.
typedef enum PshKind {
    PshKind_Short, // 10-byte sample records, sequencer shared with PshKind_Long
    PshKind_Long,  // 32-byte sample records, envelope tables
    PshKind_Old,   // 32-byte sample records, instrument table, own sequencer
} PshKind;

// What an order list code does; the layouts order the codes differently.
typedef enum PshOrderAction {
    PshOrderAction_Stop,
    PshOrderAction_Loop,
    PshOrderAction_VolumeSlide,
    PshOrderAction_AllVolumeSlide,
    PshOrderAction_PitchOffset,
    PshOrderAction_PitchTune,
} PshOrderAction;

// Everything that tells the layouts apart.
typedef struct PshLayout {
    PshKind kind;
    uint32_t sample_record_size;
    uint32_t tables_offset; // the table words of the file header
    uint32_t table_count;
    uint32_t voices_offset; // the (start volume, order list) pairs; the header ends behind them
    uint32_t table_marker;  // the long the first table word points at; 0: the word before the first order list instead
    uint8_t next_pattern_command; // the pattern byte that reads the next order list entry
    uint32_t loop_channel;        // the order list loop of this channel is the eagleplayer's SongEnd
    PshOrderAction order_actions[PSH_ORDER_CODE_COUNT];
} PshLayout;

// One entry of the sample table: what a sample command copies into the voice.
typedef struct PshSample {
    uint32_t start; // chip offset
    uint16_t length_words;
    uint32_t loop; // chip offset
    uint16_t loop_length_words;
    uint16_t tune;   // added to the period index of every note
    uint16_t volume; // the voice's volume level at a note start, unless the song overrides it
} PshSample;

// What the header says about one Paula channel.
typedef struct PshVoiceHeader {
    uint16_t volume;
    int32_t order_list; // offset
} PshVoiceHeader;

typedef struct PshModule {
    const PshLayout* layout;
    uint8_t* chip;
    uint32_t size;
    uint32_t stream_idle;   // the two bytes a voice reads before its first order list entry
    uint32_t envelope_idle; // the byte an idle envelope reads
    uint32_t wave_base;     // the replayer's data hunk
    uint32_t silence;       // a zero sample in it
    uint32_t sample_base;   // where the sample bytes start
    uint32_t idle_sample;   // what a voice plays before its first sample command
    PshSample samples[PSH_SAMPLE_COUNT + PSH_WAVE_COUNT];
    uint32_t sample_count;
    int32_t pitch_envelopes;           // table of envelope offsets
    int32_t volume_envelopes;          // table of envelope offsets
    int32_t instruments;               // PshKind_Old: table of instrument records
    PshVoiceHeader voices[PSH_VOICES]; // by Paula channel
} PshModule;

typedef struct PshVoice {
    bool active;
    bool releasing;      // the note has ended: the volume level falls
    bool direct_period;  // PshKind_Short/Long: the period register holds a converted period, not a note
    bool dma_pending;    // DMA on at the start of the next tick
    bool repeat_pending; // loop pointer written once that DMA started
    int16_t note_offset; // added to every converted period
    int16_t pitch_tune;  // set by the order list, added to the note
    int16_t sample_tune; // from the sample table, added to the note
    uint32_t sample;     // chip offset of AUDxLC
    uint16_t length_words;
    uint32_t loop;
    uint16_t loop_length_words;
    int32_t order_restart; // offset of the list's first entry
    int32_t order_next;    // offset of the next entry
    int32_t pattern;       // offset of the current pattern
    int32_t stream;        // offset of the next pattern byte
    uint16_t slide_rate;   // ticks between two steps of the volume slide
    uint16_t slide_counter;
    uint16_t volume_target;
    uint16_t volume_current;
    uint16_t volume_override; // non-zero: the level a note starts with
    uint16_t level;           // the volume envelope's level
    uint16_t sample_volume;
    uint16_t period; // note * 8 or a converted period, integer part
    uint16_t period_fraction;
    uint32_t period_slide;    // 16.16 added to the period every tick, modulo 2^32
    uint16_t duration;        // ticks until the next pattern command
    uint16_t pattern_repeats; // times the pattern still plays
    int32_t pitch_restart;    // pitch envelope: first byte
    int32_t pitch_next;
    uint8_t pitch_interval;
    uint8_t pitch_counter;
    int32_t volume_restart; // volume envelope
    int32_t volume_next;
    uint8_t volume_interval;
    uint8_t volume_counter;
    int display_note;
    int display_instrument;
} PshVoice;

struct PshPlayer {
    PshModule module;
    PshVoice voices[PSH_VOICES]; // by Paula channel
    NuadePaulaChannel paula[PSH_VOICES];
    bool song_end; // the eagleplayer's SongEnd call happened in this tick
};

// The eagleplayer's Check2 and the size check of its InitPlayer: false when either would fail.
bool psh_probe(const uint8_t* data, size_t size);

// Copies the module; null when psh_probe fails.
PshPlayer* psh_create(const uint8_t* data, size_t size);
void psh_destroy(PshPlayer* player);

// (Re)start the song from its beginning.
bool psh_start(PshPlayer* player, NuadeVoices* voices);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
PshTickResult psh_tick(PshPlayer* player, NuadeVoices* voices);

PshVoiceState psh_voice_state(const PshPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PshPlayer* psh_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
