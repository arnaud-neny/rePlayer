// SPDX-License-Identifier: MIT
// PumaTracker player

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

#define PUMA_VOICES 4

// The CIA clock: the timebase of PUMA_TICK_LENGTH.
#define PUMA_TIMEBASE_HZ 709379
// One player tick (CIA timer value $376B, 50 Hz), in 1 / PUMA_TIMEBASE_HZ seconds.
#define PUMA_TICK_LENGTH 14187

#define PUMA_SAMPLE_COUNT 10
#define PUMA_MAX_TRACKS 256
#define PUMA_MAX_INSTRUMENTS 66

typedef struct PumaPlayer PumaPlayer;

typedef struct PumaTickResult {
    bool looped; // the song passed its last position during this tick
} PumaTickResult;

typedef struct PumaPosition {
    int order; // song position
    int row;   // 0 .. 31
    int speed; // ticks per row
} PumaPosition;

typedef struct PumaVoiceState {
    int note;       // last note started on the voice: byte offset into the period table with the position's
                    // transpose added, -1 if none
    int instrument; // its instrument, 0-based, -1 if none
} PumaVoiceState;

// The track entry that starts on a row, the bytes as stored; zero on a row inside an entry's duration.
typedef struct PumaCell {
    uint8_t note;       // byte offset into the period table, 0 = none
    uint8_t instrument; // 1-based, 0 = none
    uint8_t effect;     // top three bits of the instrument byte
    uint8_t effect_arg;
} PumaCell;

typedef enum PumaSourceRegion {
    PumaSourceRegion_None = 0,
    PumaSourceRegion_Module,
    PumaSourceRegion_Waveforms,
} PumaSourceRegion;

// A place in the module or in the waveform table
typedef struct PumaSampleRef {
    PumaSourceRegion region;
    int64_t offset;
} PumaSampleRef;

typedef struct PumaInstrument {
    int64_t volume_seq;
    int64_t frequency_seq;
} PumaInstrument;

// The module as loaded, with the tables InitPlay builds from it
typedef struct PumaModule {
    uint8_t* data;
    size_t size;
    uint16_t positions;
    uint32_t sample_offset[PUMA_SAMPLE_COUNT];
    uint16_t sample_length[PUMA_SAMPLE_COUNT]; // words
    int64_t tracks[PUMA_MAX_TRACKS];
    uint32_t track_count;
    PumaInstrument instruments[PUMA_MAX_INSTRUMENTS];
    uint32_t instrument_count;
} PumaModule;

typedef struct PumaVoice {
    PumaSampleRef sample; // sample or waveform the volume sequence selected
    uint16_t length;      // words
    uint16_t period;
    uint8_t volume;      // from the volume sequence
    int64_t track_pos;   // the current track entry
    uint8_t rows_left;   // rows until the next track entry
    uint8_t note_volume; // base volume of the note
    uint8_t instrument_transpose;
    uint8_t note_transpose;
    int64_t volume_seq;    // instrument's volume sequence
    int64_t frequency_seq; // instrument's frequency sequence
    bool volume_fresh;     // the next ramp command starts instead of continuing
    bool frequency_fresh;  // the next ramp or hold command starts instead of continuing
    bool playing;
    bool set_length_one; // after the next register write the channel repeats a single word
    uint8_t volume_pos;  // offsets into the sequences
    uint8_t frequency_pos;
    uint8_t volume_ticks;    // ticks left in the volume ramp
    uint8_t volume_accum;    // ramp error term
    uint8_t volume_delta;    // ramp size
    uint8_t volume_dir;      // 1 or 0xFF
    uint8_t frequency_ticks; // ticks left in the frequency ramp or hold
    uint32_t frequency_acc;  // 16.16 period of the ramp
    int32_t frequency_step;  // 16.16 per tick
    uint8_t wave_step;       // waveform animation: waveform change per tick (signed), 0 = off
    uint8_t wave_cur;        // current waveform offset (signed)
    uint8_t wave_count;      // turning point
    uint16_t porta;          // added to the period every tick
    uint8_t note;            // note plus note transpose, byte offset into k_periods
    int shown_note;          // note of the last entry that started one, -1 if none
    int shown_instrument;    // its instrument number (1-based, after the transpose), 0 if none
    NuadePaulaChannel paula;
} PumaVoice;

typedef struct PumaSong {
    PumaVoice voices[PUMA_VOICES];
    uint8_t tick_counter;
    uint8_t row;
    uint8_t position;
    uint8_t speed;
    // The row whose entries the last row tick read. The position counter moves on one tick after the last row's
    // tick, while that row is still playing.
    uint8_t shown_position;
    uint8_t shown_row;
    bool looped;
} PumaSong;

struct PumaPlayer {
    PumaModule mod;
    PumaSong song;
};

// True when the data has a position table followed by a 'patt' marker and a first track of 32 rows.
bool puma_probe(const uint8_t* data, size_t size);

PumaPlayer* puma_create(const uint8_t* data, size_t size);
void puma_destroy(PumaPlayer* player);

// The song name stored in the header (12 characters, not terminated).
const char* puma_title(const PumaPlayer* player);

// (Re)start from the first position; the player drives `voices` (Paula channel n = voice n).
bool puma_start(PumaPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
PumaTickResult puma_tick(PumaPlayer* player, NuadeVoices* voices);

// The row whose entries were read by the last row tick (position 0, row 0 before the first one).
PumaPosition puma_position(const PumaPlayer* player);
int puma_position_count(const PumaPlayer* player);

PumaVoiceState puma_voice_state(const PumaPlayer* player, int voice);

// `position` 0 .. puma_position_count - 1, `voice` 0..3, `row` 0..31.
PumaCell puma_pattern_cell(const PumaPlayer* player, int position, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
PumaPlayer* puma_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
