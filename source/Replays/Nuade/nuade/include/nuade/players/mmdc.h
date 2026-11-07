// SPDX-License-Identifier: MIT
// MMDC player (OctaMED MMD0 modules with run-length coded blocks, "mmdc.name"), translated from the UADE eagleplayer
// "MMDC".

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

#define MMDC_VOICES 4

// The CIA clock: the timebase of the tick length.
#define MMDC_TIMEBASE_HZ 709379
// Ticks per pattern line, fixed by the replayer.
#define MMDC_TICKS_PER_LINE 6

#define MMDC_MAX_TRACKS 4
#define MMDC_MAX_POSITIONS 256
#define MMDC_MAX_BLOCKS 255
#define MMDC_INSTRUMENTS 63
#define MMDC_TRACK_VOLUMES 16
#define MMDC_CELL_SIZE 3

typedef struct MmdcPlayer MmdcPlayer;

typedef struct MmdcTickResult {
    bool looped;     // the replayer called SongEnd
    uint32_t length; // ticks until the next tick, in 1 / MMDC_TIMEBASE_HZ seconds
} MmdcTickResult;

typedef struct MmdcVoiceState {
    int note;       // semitones from C-0 of the last note started, -1 if none
    int instrument; // 0-based, -1 if none
} MmdcVoiceState;

typedef struct MmdcPosition {
    uint32_t order; // position in the play sequence
    uint32_t block;
    uint32_t line;  // the line read last (0 before the first)
    uint32_t lines; // lines of the block
    uint32_t tick;  // within the line
} MmdcPosition;

// A cell as stored; no transposes applied.
typedef struct MmdcCell {
    uint8_t note;       // 1..63 period table index, 0 none
    uint8_t instrument; // 1-based, 0 none
    uint8_t effect;
    uint8_t param;
} MmdcCell;

// The block played at a position of the play sequence (limited to the blocks the song has); false past the sequence.
typedef struct MmdcBlockShape {
    uint32_t block;
    uint32_t lines;
} MmdcBlockShape;

typedef enum MmdcEffect {
    MmdcEffect_Arpeggio = 0x0,
    MmdcEffect_SlideUp = 0x1,
    MmdcEffect_SlideDown = 0x2,
    MmdcEffect_TonePortamento = 0x3,
    MmdcEffect_Vibrato = 0x4,
    MmdcEffect_SlideUpFirstTicks = 0x5, // lowers the period by the parameter on the first three ticks of a line
    MmdcEffect_VolumeSlideA = 0xA,
    MmdcEffect_PositionJump = 0xB,
    MmdcEffect_SetVolume = 0xC,
    MmdcEffect_VolumeSlideD = 0xD,
    MmdcEffect_Misc = 0xF,
} MmdcEffect;

typedef enum MmdcPositionRequest {
    MmdcPositionRequest_None = 0,
    MmdcPositionRequest_Jump = 1,         // the position was set by a position jump
    MmdcPositionRequest_NextBlock = 0xFF, // go on with the next position
} MmdcPositionRequest;

typedef struct MmdcVoice {
    NuadePaulaChannel paula;

    uint8_t note;          // the last note
    uint8_t instrument;    // 0-based
    uint8_t volume;        // 0..64
    MmdcEffect effect;     // of the current line
    uint8_t param;         // its parameter
    uint8_t slide_speed;   // tone portamento speed
    int16_t period;        // the period the slides start from
    uint16_t slide_target; // tone portamento target, 0 when it has arrived
    uint8_t vibrato_pos;
    uint8_t vibrato_param; // speed in the high nibble, depth in the low one

    // What the end of the tick hands to Paula: the DMA start and the repeat the sample continues with
    bool dma_pending;
    const int8_t* repeat_lc;
    uint16_t repeat_words;

    int shown_note; // display: semitones from C-0 of the last note started, -1 if none
} MmdcVoice;

// The module in memory. A read that is not fully inside gives 0.
typedef struct MmdcImage {
    const uint8_t* data;
    uint32_t size;
} MmdcImage;

// One instrument of the song structure.
typedef struct MmdcInstrument {
    uint16_t repeat_start; // words
    uint16_t repeat_words;
    uint8_t volume; // 0..64
    int8_t transpose;
} MmdcInstrument;

// A sample record: the length in bytes, then the 8-bit data after a 2-byte type word.
typedef struct MmdcSample {
    bool present;
    uint32_t data;   // offset of the first data byte
    uint32_t length; // bytes
} MmdcSample;

// A block: its track count and line count, then the run-length coded cells.
typedef struct MmdcBlock {
    uint32_t offset;
    uint8_t tracks;
    uint16_t lines;
} MmdcBlock;

// The song structure.
typedef struct MmdcSong {
    MmdcInstrument instruments[MMDC_INSTRUMENTS];
    uint16_t block_count;
    uint16_t length; // positions of the play sequence
    uint8_t play_sequence[MMDC_MAX_POSITIONS];
    uint16_t tempo; // at the start of the song
    int8_t play_transpose;
    bool st_slides;
    uint8_t slide_limit; // the second tempo value: the tick from which the ST slides wait
    uint8_t track_volumes[MMDC_TRACK_VOLUMES];
    uint8_t master_volume;
} MmdcSong;

// Everything the replayer reads from the module, read once by mmdc_parse_module.
typedef struct MmdcModule {
    MmdcSong song;
    MmdcBlock blocks[MMDC_MAX_BLOCKS]; // song.block_count of them
    uint32_t sample_count;             // slots of the sample array that instruments can name
    MmdcSample samples[MMDC_INSTRUMENTS];
} MmdcModule;

// Decodes the lines of a block in order. The coding: a stream byte below 0x80 starts a run of that many + 1 stream
// bytes copied as they are, a byte of 0x80 or more starts a run of (~byte) + 1 zero bytes; runs cross lines.
typedef struct MmdcLineReader {
    const MmdcImage* image;
    uint32_t pos; // next byte of the stream
    int run_left; // bytes the current run still produces; negative: the next stream byte starts a run
    bool literal; // the run copies stream bytes; otherwise it produces zeros
} MmdcLineReader;

struct MmdcPlayer {
    MmdcImage image;
    MmdcModule module;
    uint8_t* memory;      // a copy of the module and PADDING_BYTES of zeros
    int8_t loop_dummy[4]; // what a sample without a repeat loops on after its end: the zeroed data hunk of the replayer

    MmdcVoice voices[MMDC_VOICES];

    bool playing; // false after the Stop command: the next tick calls SongEnd and restarts the song
    MmdcPositionRequest position_request;
    uint8_t tick;
    uint16_t line;
    uint16_t position;
    uint16_t block_index;
    MmdcBlock block;
    MmdcLineReader reader;
    uint8_t cells[MMDC_MAX_TRACKS * MMDC_CELL_SIZE];

    uint16_t song_tempo; // the start tempo with its low byte replaced by every tempo command
    uint8_t slide_limit; // the tick from which the ST slides wait; set by the tempo commands
    uint16_t timer;      // the CIA timer value in effect when the next tick starts
    bool looped;
};

// The eagleplayer's Check2 and InitPlayer checks, and modules the replayer would read past (see mmdc_module.h).
bool mmdc_probe(const uint8_t* data, size_t size);

// Null when the module is refused or on out of memory.
MmdcPlayer* mmdc_create(const uint8_t* data, size_t size);
void mmdc_destroy(MmdcPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mmdc_start(MmdcPlayer* player, NuadeVoices* voices);

MmdcTickResult mmdc_tick(MmdcPlayer* player, NuadeVoices* voices);

MmdcVoiceState mmdc_voice_state(const MmdcPlayer* player, uint32_t voice);

MmdcPosition mmdc_position(const MmdcPlayer* player);

bool mmdc_block_shape(const MmdcPlayer* player, uint32_t order, MmdcBlockShape* shape);
MmdcCell mmdc_pattern_cell(const MmdcPlayer* player, uint32_t order, uint32_t voice, uint32_t line);

// The player's state behind a song it opened; 0 when the song is another player's.
MmdcPlayer* mmdc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
