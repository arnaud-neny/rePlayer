// SPDX-License-Identifier: MIT
// Major Tom player ("Arpeggiator", ARP files), translated from the UADE eagleplayer "MajorTom". A song is one file: the
// "ARP." mark, a table of offsets, the song and position lists, the instruments (four sequence numbers each), the
// sequence blocks (volume, waveform, note offset, period offset), the 32-byte waveforms and the 64-row patterns. The
// eagleplayer holds the replayer; the noise waveform is its own data.

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

#define MT_VOICES 4
#define MT_ROWS 64

// The CIA clock: the timebase of MT_TICK_LENGTH.
#define MT_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MT_TIMEBASE_HZ seconds.
#define MT_TICK_LENGTH 14187

#define MT_SEQUENCES 4

typedef struct MtPlayer MtPlayer;

typedef struct MtTickResult {
    bool looped; // the song wrapped to its restart position: the eagleplayer calls SongEnd
    bool ended;  // the song read memory the port does not model: later ticks do nothing
} MtTickResult;

typedef struct MtPosition {
    int order; // song position
    int row;
    int speed; // ticks per row
} MtPosition;

typedef struct MtVoiceState {
    int note;       // the pattern note of the last note started, plus the position's transpose; 0 if none
    int instrument; // that note's instrument, -1 if none
} MtVoiceState;

// One pattern cell as stored.
typedef struct MtCell {
    uint8_t note; // 0 none; else (octave + 1) * 16 + semitone
    uint8_t instrument;
    uint8_t effect;
    uint8_t param;
} MtCell;

typedef enum MtDirection {
    MtDirection_None = 0,
    MtDirection_Up, // the period grows
    MtDirection_Down,
} MtDirection;

// A song in the memory of the replayer: the file followed by zeros (the padded copy of nuade_paula_padded_copy).
typedef struct MtModule {
    uint8_t* bytes;
    uint32_t size;
} MtModule;

// A sequence block: the first byte is the ticks per step, the steps follow. A voice that has not played a note yet
// reads a block of zeros.
typedef struct MtBlock {
    bool zeros;
    int64_t offset;
} MtBlock;

typedef struct MtSequenceState {
    MtBlock block;
    uint32_t index;
    uint8_t timer; // ticks until the next step
    uint8_t rate;  // ticks per step, the first byte of the block
    uint32_t hold; // the index a sequence stays on at the end of its block
} MtSequenceState;

typedef struct MtVoice {
    NuadePaulaChannel paula;
    uint8_t note; // the pattern note plus the position's transpose
    uint8_t effect;
    uint8_t param;
    MtSequenceState sequence[MT_SEQUENCES];
    bool noise;       // AUDxLC holds the noise table
    bool new_note;    // a note started in this tick's row: the sequences wait for the next tick
    int16_t slide;    // the slide effects' sum
    uint16_t current; // the period the portamento has reached
    uint16_t target;  // the period of the note
    MtDirection direction;
    uint8_t note_offset;      // the note sequence's value
    uint8_t note_offset_used; // the note offset the last tick of the voice took the period from
    uint8_t volume_step;      // the volume register as last written
    uint32_t sample_address;  // the Amiga address of the waveform last started
    int shown_instrument;
} MtVoice;

struct MtPlayer {
    MtModule module;
    uint8_t* noise; // the noise table followed by zeros (the padded copy): AUDxLC of the voices points into it
    uint16_t speed_counter;
    uint8_t position;
    uint32_t row_offset; // bytes into the pattern
    bool unmapped;       // the replayer read memory the port does not model
    bool ended;
    uint16_t file_speed; // the speed word of the file, which the speed effect overwrites
    MtVoice voices[MT_VOICES];
};

// The eagleplayer's Check2 (the "ARP." mark) plus what this port needs: the table of offsets the songs use, and a run
// of the whole song (two passes) that reads nothing outside the memory the port models.
bool mt_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `mt_probe` rejects it.
MtPlayer* mt_create(const uint8_t* data, size_t size);
void mt_destroy(MtPlayer* player);

// (Re)start the song from its beginning; the player drives `voices` (Paula channel n = voice n).
bool mt_start(MtPlayer* player, NuadeVoices* voices);

MtTickResult mt_tick(MtPlayer* player, NuadeVoices* voices);

// The row the next row tick decodes.
MtPosition mt_position(const MtPlayer* player);
int mt_order_count(const MtPlayer* player);

MtVoiceState mt_voice_state(const MtPlayer* player, int voice);

// The pattern of song position `order`, `voice` 0..3, `row` 0 .. MT_ROWS - 1.
MtCell mt_pattern_cell(const MtPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
MtPlayer* mt_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
