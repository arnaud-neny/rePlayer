// SPDX-License-Identifier: MIT
// Richard Joseph Player (the music driver of the Amiga games by Richard Joseph, Sensible Software and others;
// "rjp.name" + "smp.name" or "name.sng" + "name.ins"), translated from the UADE eagleplayer "RichardJoseph_Player".

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

#define RJP_VOICES 4

// The CIA clock: the timebase of RJP_TICK_LENGTH.
#define RJP_TIMEBASE_HZ 709379
// DeliTracker's default 50 Hz tick, in 1 / RJP_TIMEBASE_HZ seconds.
#define RJP_TICK_LENGTH 14187

typedef struct RjpPlayer RjpPlayer;

typedef enum RjpStatus {
    RjpStatus_Ok = 0,
    RjpStatus_InvalidData,
    RjpStatus_MissingSamples, // neither the sample file next to the song nor "smp.set" could be read, or no path
    RjpStatus_OutOfMemory,
} RjpStatus;

typedef struct RjpCreateResult {
    RjpPlayer* player;
    RjpStatus status;
} RjpCreateResult;

typedef struct RjpTickResult {
    bool looped; // the driver called SongEnd: every voice that has a sequence wrapped or stopped since the last call
    bool ended;  // every voice has stopped
} RjpTickResult;

typedef struct RjpVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // instrument number of the last instrument command, -1 if none
} RjpVoiceState;

typedef struct RjpImage {
    uint8_t* data;
    uint32_t size;
} RjpImage;

typedef struct RjpSection {
    uint32_t offset; // of the data in the module
    uint32_t size;
} RjpSection;

typedef struct RjpModule {
    RjpImage image;
    RjpSection instruments;      // 32-byte records
    RjpSection envelopes;        // 6-byte volume envelopes, addressed by byte offset
    RjpSection songs;            // 4 bytes per subsong: the sequence number of each voice, 0 for none
    RjpSection sequence_offsets; // longs, per sequence number: the offset of a pattern list in `sequences`
    RjpSection pattern_offsets;  // longs, per pattern number: the offset of its commands in `patterns`
    RjpSection sequences;        // pattern lists: pattern numbers, then 0 and a loop or jump
    RjpSection patterns;         // notes and commands
    uint32_t subsong_count;
} RjpModule;

// One instrument record (RjpModule.instruments), with its sample pointers relocated: offsets into the sample image.
typedef struct RjpInstrument {
    uint32_t sample;
    bool has_pitch_lfo;
    uint32_t pitch_lfo;
    bool has_volume_lfo;
    uint32_t volume_lfo;
    int16_t envelope;           // byte offset in RjpModule.envelopes
    uint16_t volume;            // 0..64 normally
    uint16_t attack_offset;     // bytes from `sample`
    uint16_t attack_words;      // the part of the sample that plays first
    uint16_t loop_offset_words; // from `sample`
    uint16_t loop_words;        // the part that repeats
    uint16_t pitch_lfo_restart; // words
    uint16_t pitch_lfo_words;
    uint16_t volume_lfo_restart;
    uint16_t volume_lfo_words;
} RjpInstrument;

typedef enum RjpEnvelopeStage {
    RjpEnvelopeStage_Idle = 0,
    RjpEnvelopeStage_Attack,
    RjpEnvelopeStage_Decay,
    RjpEnvelopeStage_Release,
} RjpEnvelopeStage;

typedef struct RjpEnvelopeState {
    RjpEnvelopeStage stage;
    int8_t step;       // the distance the ramp covers
    uint8_t remaining; // ticks left in the ramp
    uint8_t duration;  // ticks of the ramp
    uint8_t target;    // the level the ramp ends at
    uint8_t level;
    bool has_block; // a note has started: the block below is valid
    uint32_t block; // offset of the envelope block in the module
} RjpEnvelopeState;

// A low frequency oscillator over a table of signed bytes in the sample image.
typedef struct RjpLfo {
    bool enabled;
    uint32_t table;
    uint32_t restart; // byte position after the end
    uint32_t end;     // byte position where it wraps
    uint32_t position;
} RjpLfo;

typedef struct RjpVoice {
    NuadePaulaChannel paula;

    // Sequencer
    bool active;           // the voice has a pattern list
    uint32_t sequence_pos; // the next entry of the pattern list, offset in the module
    uint32_t pattern_pos;  // the next command, offset in the module
    uint8_t speed;         // ticks per count
    uint8_t speed_counter;
    uint8_t row_length; // counts per row
    uint8_t row_counter;

    // Instrument
    bool has_instrument;
    uint8_t instrument_number;
    RjpInstrument instrument;
    uint16_t volume; // the instrument's volume, changed by the volume command

    // Volume
    RjpEnvelopeState envelope;
    RjpLfo volume_lfo;
    uint16_t level; // the scaled volume of the last tick (the release's last tick scales it a second time)

    // Pitch
    uint16_t period;     // of the note
    uint16_t lfo_period; // with the pitch LFO
    RjpLfo pitch_lfo;
    uint8_t slide_ticks; // ticks the slide still runs
    bool slide_keep;     // the slide command ran this tick: the next note keeps the slide
    uint32_t slide_step; // added to `slide_pos` every tick
    uint32_t slide_pos;  // 16.16: the upper word is added to the period

    // Paula hand-over
    bool dma_pending;  // a note started: DMA goes on next tick
    bool loop_pending; // DMA went on: the loop registers are written next tick

    int note; // display: semitones from C-0 of the last note, -1 if none
} RjpVoice;

struct RjpPlayer {
    RjpModule module;
    // The working copy, a nuade_paula_padded_copy of `samples_end` bytes: the driver clears the first word of every
    // attack it starts
    RjpImage samples;
    uint8_t* samples_original;
    RjpVoice voices[RJP_VOICES];
    bool wrap_pending[RJP_VOICES]; // the voice has not wrapped since SongEnd; its sequence exists
    bool wrap_initial[RJP_VOICES];
    uint32_t sequence_count; // voices that start with a pattern list
    uint32_t voices_active;
    bool looped;
};

// The eagleplayer's Check2 (the "RJP?SMOD" header; not its zero long in the first instrument record) and the layout
// InitPlayer needs. Whether the sample file exists is only known to rjp_create.
bool rjp_probe(const uint8_t* data, size_t size);

// Parses the module (the eagleplayer's InitPlayer) and loads its sample file from the directory of `path`.
RjpCreateResult rjp_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void rjp_destroy(RjpPlayer* player);

uint32_t rjp_subsong_count(const RjpPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool rjp_start(RjpPlayer* player, uint32_t subsong, NuadeVoices* voices);

RjpTickResult rjp_tick(RjpPlayer* player, NuadeVoices* voices);

// Ticks one sequencer row spans at the speed every voice starts with.
uint32_t rjp_grid_ticks(void);

RjpVoiceState rjp_voice_state(const RjpPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
RjpPlayer* rjp_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
