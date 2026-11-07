// SPDX-License-Identifier: MIT
// Art and Magic music driver (.aam): the modules of Jeroen Tel's Agony tunes are relocatable 68k programs that carry
// the driver, its song tables and 8SVX samples; this is a translation of the driver.

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

// Paula channels. The driver plays on channels 1..3; channel 0 is never written.
#define AAM_VOICES 4

// The CIA clock: the timebase of AAM_TICK_LENGTH.
#define AAM_TIMEBASE_HZ 709379
// One driver tick: DeliTracker's default 50 Hz interrupt, in 1 / AAM_TIMEBASE_HZ seconds.
#define AAM_TICK_LENGTH 14187

typedef struct AamPlayer AamPlayer;

typedef struct AamTickResult {
    bool looped; // a voice's track list started over during this tick (the eagleplayer's song end)
    bool ended;  // every voice stopped: the driver does nothing more
} AamTickResult;

// What voice (0-based) last started: the note in semitones from C-0 (-1 if none yet) and the instrument (-1 if none).
typedef struct AamVoiceState {
    int note;
    int instrument;
} AamVoiceState;

// Ticks of the DMA start sequence of a note: DMA on one tick after the note, the repeat registers one tick later.
typedef enum AamDmaStage {
    AamDmaStage_Idle = 0,
    AamDmaStage_SetRepeat = 1,
    AamDmaStage_Start = 2,
} AamDmaStage;

// Where the driver's tables are (file offsets).
typedef struct AamLayout {
    uint32_t first_form;   // the first 8SVX form of the sample area
    uint32_t sample_table; // 12 bytes per sample: start, repeat start (absolute addresses), length
    uint32_t voice_table;  // per voice: track list, two swing lengths. Followed by the relocated pointer table
    uint32_t patterns;     // pointers to the note streams (index 0 .. as many as the file has; more reads on)
    uint32_t pitch_seqs;   // pointers to the pitch sequences (32)
    uint32_t instruments;  // 8 bytes each
    uint32_t periods;      // 96 words
    uint32_t first_note;   // a note stream that holds a pattern end: where the voices start
    uint32_t origin;       // the stored address the relocation counts from (as a file offset)
    uint32_t pointers_end; // end of the pointer table the eagleplayer relocates
} AamLayout;

// The driver's record of a voice.
typedef struct AamVoice {
    bool active;
    uint8_t channel; // Paula channel, and the voice's entry in the voice table

    uint32_t track_pos;      // next byte of the track list
    int8_t pattern;          // number of the running pattern
    uint8_t pattern_repeats; // times it still plays after this one
    uint8_t note_base;       // note transposition
    uint8_t instrument_base; // instrument offset
    uint32_t note_pos;       // next byte of the note stream

    uint16_t swing_ticks_left; // ticks until the next step
    bool swing_odd;            // the step now running is the second of its pair
    uint8_t duration;          // steps of a note
    uint8_t steps_left;
    int8_t note; // index into the period table; negative for a rest
    uint8_t instrument;
    uint8_t sample;
    AamDmaStage dma_stage;

    uint32_t pitch; // 16.16: the period in the high word

    int16_t slide_from; // the slide runs while at most this many steps are left (-1: none)
    int8_t slide_speed;
    int8_t slide_target; // note

    int16_t vibrato_from; // the vibrato runs while fewer than this many steps are left
    uint8_t vibrato_shift;
    int8_t vibrato_counter;
    bool vibrato_down;

    bool pitch_seq_on;
    uint8_t pitch_seq; // number in the pointer table
    uint32_t pitch_seq_pos;

    uint16_t level_base; // the note's volume
    uint16_t level;      // the volume now (the envelope shapes it)
    bool fade_in;
    int16_t fade_out_from; // the volume fades out while fewer than this many steps are left (-1: no)
    uint16_t ramp;         // the envelope's position, 0x200 down to 0

    // Display only
    int shown_note;
    int shown_instrument;

    NuadePaulaChannel paula;
} AamVoice;

struct AamPlayer {
    uint8_t* image; // the file as the driver sees memory after its init code: samples compacted. Padded with zeros
    size_t size;
    AamLayout layout;
    AamVoice voices[AAM_VOICES];
    bool looped;
};

// True when the data passes the eagleplayer's module check: the "EXIT" tag of the init code and of the file's end.
bool aam_probe(const uint8_t* data, size_t size);

// Null when the data is not a module the driver's code layout of any known build fits.
AamPlayer* aam_create(const uint8_t* data, size_t size);
void aam_destroy(AamPlayer* player);

// (Re)start the song; the player drives `voices` (Paula channel n = voice n).
bool aam_start(AamPlayer* player, NuadeVoices* voices);

// Run one driver tick and hand the Paula registers it wrote to the voices.
AamTickResult aam_tick(AamPlayer* player, NuadeVoices* voices);
AamVoiceState aam_voice_state(const AamPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
AamPlayer* aam_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
