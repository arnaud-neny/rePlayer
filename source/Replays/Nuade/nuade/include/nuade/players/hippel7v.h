// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player, translated from the replayer in UADE's "Jochen Hippel 7V player module V1.3" (Hippel's
// code, adapted by the Wanted Team): three voices play through their own Paula channels, four more are mixed in
// software into the fourth.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/players/hippel.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

// Paula channels the player drives
#define H7V_VOICES 4
// The tracks of the song: three Paula channels and the four mixed voices
#define H7V_TRACKS 7

// The PAL colour clock: the timebase of the tick length (a tick is the audio interrupt at the end of a mix buffer).
#define H7V_TIMEBASE_HZ 3546895

#define H7V_MIXED_TRACKS 4 // tracks 3..6 are mixed into Paula channel 0
// Mix buffers of the replayer: two of this many bytes
#define H7V_BUFFER_BYTES 0x258

typedef struct H7vPlayer H7vPlayer;

typedef struct H7vTickResult {
    uint32_t length; // until the next tick, in H7V_TIMEBASE_HZ units
    bool looped;     // a track wrapped to the subsong start during this tick
    bool ended;      // the song stopped itself (track command 8): the audio DMA is off
} H7vTickResult;

typedef struct H7vTrackState {
    int note;             // period table index of the note last started, -1 if none
    int instrument;       // volume sequence (instrument) number, -1 if none
    uint32_t note_starts; // notes started since the subsong began
} H7vTrackState;

typedef struct H7vModule {
    uint8_t*
        data; // owned, writable (sample packs patch their first byte); NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes follow
    uint32_t size;
    // Module offsets of the tables that follow the "TFMX" header
    uint32_t fseqs;       // frequency sequences, 64 bytes each
    uint32_t vseqs;       // volume sequences (instruments), 64 bytes each
    uint32_t patterns;    // patterns, `pattern_size` bytes each
    uint32_t tracks;      // track positions, 7 columns of 4 bytes
    uint32_t subsongs;    // 8 bytes each
    uint32_t samples;     // sample records, 30 bytes each
    uint32_t sample_data; // the samples
    uint32_t sample_count;
    uint16_t pattern_size;
    uint16_t subsong_count;
} H7vModule;

// A frequency or volume sequence: in the module, or the replayer's start sequence.
typedef struct H7vSequence {
    const uint8_t* data;
    uint32_t size;
    uint32_t before; // bytes readable in front of `data` (a volume sequence jump can go back into the header)
} H7vSequence;

typedef struct H7vVoice {
    int mixed_slot;     // -1 for a Paula channel, else 0..3: the mixer voice
    bool restart_dma;   // a Paula channel that stopped for a new sample in this tick: DMA goes on at its end
    bool mixer_on;      // a mixed voice the mixer plays
    bool mixer_started; // a mixed voice that started a sample once: every tick switches it on again

    // Track list
    uint32_t track_start; // module offset of this track's entry in the first position of the subsong
    uint32_t track_end;   // ... in the position after the last one
    uint16_t track_pos;
    uint32_t pattern; // module offset of the current pattern
    uint16_t pattern_pos;
    uint8_t note_transpose;
    uint8_t instrument_transpose;
    uint8_t volume_percent; // 0..100

    // The note
    uint8_t note;
    uint8_t info;      // instrument (bits 0-4), portamento (5), alternative frequency sequence (6)
    uint8_t info_prev; // the info byte of the row before: portamento speed, or the alternative frequency sequence
    uint16_t porta;
    int last_note; // period table index of the note last started, -1 if none (for the display)
    int last_instrument;
    uint32_t note_starts;

    // Frequency sequence
    H7vSequence fseq;
    uint16_t fseq_pos;
    uint8_t fseq_wait;
    uint8_t fseq_value;    // note offset, or an absolute note with bit 7 set
    uint8_t sample_number; // 0xFF = none / forced restart
    const int8_t* loop;    // AUDxLC for the repeat
    uint16_t loop_length;  // words

    // Sample window sweep
    bool sweep_active;
    bool sweep_stopped;
    bool sweep_started;
    uint32_t sweep_start; // sample data offsets
    uint32_t sweep_end;
    uint16_t sweep_pos; // words
    int16_t sweep_delta;
    uint16_t sweep_length;
    int8_t sweep_counter;
    uint8_t sweep_speed;

    // Volume sequence
    H7vSequence vseq; // from the first value, behind the instrument's five header bytes
    uint16_t vseq_pos;
    uint8_t vseq_counter;
    uint8_t vseq_speed;
    uint8_t vseq_wait;
    uint8_t volume;

    HipVibrato vibrato;

    // Random volume reduction (command EA): the range, and the value drawn for the note
    uint8_t random_range;
    uint8_t random_value;

    // AUDxLC / AUDxLEN / AUDxPER / AUDxVOL of the Paula channel, or of the mixer voice
    NuadePaulaChannel paula;
} H7vVoice;

typedef struct H7vSong {
    uint16_t speed;
    uint16_t speed_counter;
    bool stopped;
    bool looped;
    uint16_t random_state;
    H7vVoice voices[H7V_TRACKS];
} H7vSong;

typedef struct H7vMixVoice {
    uint32_t pos;      // fraction in the high word, offset from `end` in the low word (negative until the sample ends)
    const int8_t* end; // the end of the sample the voice plays
    uint32_t step;  // the position step per output sample: the integer part in the low word, the fraction in the high
    uint8_t volume; // 0..63: the row of the volume table
    bool restart;   // the voice was off: starts from the beginning when it is switched on
    const int8_t* reload_end; // end and length of the sample the voice continues with at its end
    uint16_t reload_bytes;
} H7vMixVoice;

typedef struct H7vMixer {
    int8_t buffer[2][H7V_BUFFER_BYTES];
    int next;       // the buffer the next interrupt fills
    bool hard_clip; // the sum of the voices is clipped, else it is divided by 4
    uint16_t words; // output words per interrupt
    H7vMixVoice voice[H7V_MIXED_TRACKS];
    int8_t silence[0x140];         // the sample of a voice that has none: 0x80 words
    int8_t idle[0x100];            // what a voice that never played reads
    uint8_t volume_table[64][256]; // [volume][sample byte]: the scaled sample, offset binary
    uint8_t clip_table[1024];
    NuadePaulaChannel output; // Paula channel 0
} H7vMixer;

struct H7vPlayer {
    H7vModule module;
    H7vSong song;
    H7vMixer mixer;
    uint32_t start_margin; // timebase units added to the first tick (see h7v_start)
};

// The eagleplayer's module check: a bare "TFMX" header, or a replayer in front of it (its `bra.w`, a `move.w d1,(a0)` /
// `lea` pair that finds the header).
bool h7v_probe(const uint8_t* data, size_t size);

// 0 when the data is not a module (or out of memory).
H7vPlayer* h7v_create(const uint8_t* data, size_t size);
void h7v_destroy(H7vPlayer* player);

int h7v_subsong_count(const H7vPlayer* player);

// (Re)start subsong `subsong` (0-based; the replayer numbers them from 1) for a render at `sample_rate`. Voice n of
// `voices` is Paula channel n.
bool h7v_start(H7vPlayer* player, int subsong, uint32_t sample_rate, NuadeVoices* voices);

H7vTickResult h7v_tick(H7vPlayer* player, NuadeVoices* voices);

// Ticks per sequencer step of the running subsong.
int h7v_speed(const H7vPlayer* player);

H7vTrackState h7v_track_state(const H7vPlayer* player, int track);

// The player's state behind a song it opened; 0 when the song is another player's.
H7vPlayer* h7v_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
