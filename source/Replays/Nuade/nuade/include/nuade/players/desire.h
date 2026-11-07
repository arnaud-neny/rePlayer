// SPDX-License-Identifier: MIT
// Desire music modules (Dentons, 1993-94), translated from the UADE eagleplayer "Desire" (Wanted Team). The module
// holds its own replayer; the format and the replayer are described in players/desire/.

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

#define DSR_VOICES 4

// The CIA clock: the timebase of DSR_TICK_LENGTH.
#define DSR_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / DSR_TIMEBASE_HZ seconds.
#define DSR_TICK_LENGTH 14187

#define DSR_MAX_RECORDS (DSR_VOICES + 1)

typedef struct DsrPlayer DsrPlayer;

typedef enum DsrStatus {
    DsrStatus_Ok = 0,
    DsrStatus_InvalidData,
    DsrStatus_OutOfMemory,
} DsrStatus;

typedef struct DsrCreateResult {
    DsrPlayer* player;
    DsrStatus status;
} DsrCreateResult;

typedef struct DsrTickResult {
    bool ended; // the eagleplayer's SongEnd: the stop command ran on every voice that it counts
} DsrTickResult;

// What voice (0-based) last started: the note (semitones from C-0, -1 if none yet) and the instrument number.
typedef struct DsrVoiceState {
    int note;
    int instrument;
} DsrVoiceState;

typedef struct DsrMemory {
    const uint8_t* data;
    uint32_t size;     // the module
    uint32_t capacity; // the furthest a sample can start, at least `size`; the bytes between size and capacity are 0
} DsrMemory;

typedef struct DsrVersion {
    // The later replayers have a fifth voice record for a sound effect that plays on Paula channel 3 while it runs;
    // voice 3 is muted for `effect_mute_ticks` ticks after it.
    bool has_effect;
    uint8_t effect_mute_ticks;
    bool effect_resets_envelope; // the end of the effect also stops the envelope of voice 3
    uint16_t fade_step;          // how much the fade counter drops each time a voice writes its volume
    // Sample start = (offset word << sample_shift) + sample_skip; length = length word - length_trim, in words.
    uint8_t sample_shift;
    uint16_t sample_skip;
    uint16_t length_trim;
} DsrVersion;

typedef struct DsrTables {
    uint32_t song_lists;            // bytes: per voice a list of order table indices, one per subsong
    uint32_t orders;                // words: the offset of each track
    uint32_t modulation;            // signed bytes: pitch modulation steps, 0x7F repeats the previous step
    uint32_t periods;               // DSR_PERIODS words
    uint32_t sample_offsets;        // words, one per instrument
    uint32_t sample_lengths;        // words
    uint32_t instrument_transposes; // bytes
} DsrTables;

typedef struct DsrModule {
    DsrMemory memory;
    uint8_t* owned; // the zero padded copy that `memory` points into
    DsrVersion version;
    DsrTables tables;
    uint32_t subsongs;
} DsrModule;

// What a voice does at its next step. The module holds this in one byte: 0x80 idle, other values with bit 7 running,
// 0 stop, 0x40 fade, any other value a track request (below).
typedef enum DsrMode {
    DsrMode_Idle,
    DsrMode_Running,
    DsrMode_Stop,  // silence the voice, then idle
    DsrMode_Fade,  // start the fade out
    DsrMode_Start, // start the track named by the voice's request
} DsrMode;

// A track to start: an order table index, or an entry of the voice's song list (the subsong number).
typedef struct DsrRequest {
    bool pending;
    bool from_song_list;
    uint8_t index;
} DsrRequest;

typedef struct DsrVoice {
    DsrMode mode;
    DsrRequest request; // the track to start at the next sequencer step
    int16_t pitch_base;
    // Sequencer
    uint8_t speed;      // ticks per row
    uint8_t counter;    // ticks to the next row
    uint8_t loop_count; // plays of the loop that are left
    uint16_t loop_pos;
    uint16_t pos;       // next byte of the track
    uint16_t order_pos; // the next entry of the order table
    uint16_t order_base;
    // Pitch
    uint16_t slide_speed;
    uint16_t pitch;       // slides to `target`
    uint16_t target;      // note << 4
    int16_t pitch_offset; // from the track
    uint8_t transpose;    // from the track
    uint8_t instrument_transpose;
    // Modulation
    int16_t modulation;
    uint8_t modulation_pos;
    uint8_t modulation_length;
    uint16_t modulation_base;
    // Envelope
    uint16_t volume;
    uint16_t envelope;
    uint16_t attack;
    uint16_t decay;
    uint16_t sustain;
    uint16_t release;
    uint16_t level;
    uint8_t hold;
    uint8_t hold_count;
    bool keep_envelope; // a note does not restart the envelope
    // Paula
    int dma_channel;       // the Paula channel whose DMA the voice switches
    bool dma_enabled;      // the voice may start that DMA; voice 3 may not while a sound effect has its channel
    bool dma_pending;      // switch the DMA on at the next tick
    uint8_t mute_ticks;    // voice 3: ticks left without DMA after a sound effect
    uint32_t saved_sample; // voice 3: the sample registers that the effect replaced
    uint16_t saved_length;
    int display_note;
    int display_instrument;
} DsrVoice;

struct DsrPlayer {
    DsrModule module;
    DsrVoice voices[DSR_MAX_RECORDS];
    NuadePaulaChannel channels[DSR_VOICES];
    NuadePaulaChannel dummy; // the registers that a voice writes while a sound effect has its channel
    uint16_t master_volume;
    uint16_t fade;
    bool voice_running[DSR_VOICES]; // the eagleplayer's end flags: cleared by the stop command of the voice's channel
    bool song_ended;
    int subsong;
};

// The eagleplayer's check, and the same code scan that its InitPlayer makes: true for a module whose replayer is one
// of the known versions.
bool dsr_probe(const uint8_t* data, size_t size);

DsrCreateResult dsr_create(const uint8_t* data, size_t size);
void dsr_destroy(DsrPlayer* player);

int dsr_subsong_count(const DsrPlayer* player);

// (Re)start a subsong (0-based) from its beginning; the player drives `voices` (Paula channel n = voice n).
bool dsr_start(DsrPlayer* player, int subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
DsrTickResult dsr_tick(DsrPlayer* player, NuadeVoices* voices);
DsrVoiceState dsr_voice_state(const DsrPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
DsrPlayer* dsr_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
