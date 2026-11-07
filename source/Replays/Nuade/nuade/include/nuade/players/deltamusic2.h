// SPDX-License-Identifier: MIT

#pragma once

#include <nuade/nuade.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// The player's tick rate: one dm2_tick per VBlank.
#define DM2_TICK_HZ 50

#define DM2_BLOCK_ROWS 16
// The lowest note (period 6848) sits this many octaves below C-0.
#define DM2_NOTE_OCTAVE_SHIFT (-2)

#define DM2_CHANNELS 4

typedef struct Dm2Module Dm2Module;

// What the display shows. A voice runs its own track list of blocks of 16 rows; the row shown is the one played by the
// last tick that advanced the voice, not the one coming next.
typedef struct Dm2VoiceState {
    int note; // semitones from C-0 + DM2_NOTE_OCTAVE_SHIFT octaves (note + transpose through the period table), -1 if
              // none
    int instrument; // instrument number of the note (the first is 1), -1 if none
    int order;      // track list position of the block being played
    int row;        // row of the block that was played
    int speed;      // ticks per row of the whole song
} Dm2VoiceState;

// One row of a block, as the player reads it (the transpose of the track list entry is added to the note).
typedef struct Dm2Cell {
    int note; // as in Dm2VoiceState, -1 if none
    int instrument;
    int effect; // 0 = none
    int effect_arg;
} Dm2Cell;

// The player's global record at $b8c.
typedef struct Dm2Vars {
    uint32_t inst_base;            // $02 instrument records
    uint32_t inst_offsets;         // $06 word per instrument number, the first is for number 1
    uint32_t tracks[DM2_CHANNELS]; // $0a $0e $12 $16 track lists
    uint32_t blocks;               // $1a
    uint32_t waveforms;            // $1e
    uint32_t sample_base;          // $22
    uint32_t sample_offsets;       // $26 long per sample number
    uint32_t noise_seed;           // $2a
    int8_t master_volume;          // $2e
    int8_t speed;                  // $2f ticks per row minus one
    int8_t counter;                // $30 ticks left in the row
} Dm2Vars;

// A voice record ($878, $8b4, $904, $954).
typedef struct Dm2Voice {
    uint32_t instrument;     // $06
    uint32_t track;          // $0a track list
    uint32_t block;          // $0e current block
    uint16_t track_pos;      // $12 byte position of the next track entry
    uint16_t block_pos;      // $14 byte position of the row in the block
    uint8_t table_delay;     // $16 sound table
    uint8_t table_pos;       // $17
    uint16_t period;         // $18
    uint16_t target_period;  // $1a
    uint8_t note;            // $1c
    uint8_t max_volume;      // $1d
    uint16_t pitch_bend;     // $1e
    uint16_t volume;         // $20 envelope level, 4 per Paula volume step
    uint16_t volume_pos;     // $22 byte position in the volume table
    uint8_t volume_sustain;  // $24
    uint8_t portamento;      // $25
    uint8_t vibrato_dir;     // $27
    uint16_t vibrato;        // $28
    uint8_t vibrato_delay;   // $2a
    int8_t transpose;        // $2b
    uint32_t arpeggio;       // $2c
    uint16_t arpeggio_pos;   // $30
    uint8_t sample_start;    // $33 top bit: a sample note began and the repeat is still to be set
    uint8_t vibrato_pos;     // $34 byte position in the vibrato table
    uint8_t vibrato_sustain; // $35
    uint16_t loop_pos;       // $36 byte position the track list restarts at
    uint16_t track_len;      // $38 bytes

    // Not in the player's record: what the display shows, set when a row is played.
    uint16_t shown_track_pos; // byte position in the track list of the block that row belongs to
    uint16_t shown_row;       // byte position of the row in its block
    uint8_t shown_note;       // note + transpose of the last note started
    int16_t shown_instrument; // its instrument number, -1 before the first
} Dm2Voice;

// What a Paula channel holds, and how the tick changed it.
typedef struct Dm2Paula {
    uint32_t lc;
    uint16_t len; // words, 0 is 65536
    uint16_t per;
    uint16_t vol;
    bool running; // DMA is on
    bool dma_off; // switched off this tick (a note); on again at its end
    bool lc_len_written;
} Dm2Paula;

struct Dm2Module {
    NuadeVoices* voices;

    uint8_t* file; // the module as loaded
    size_t file_size;
    uint8_t* image; // what the player works on
    size_t image_size;

    Dm2Vars vars;
    Dm2Voice voice[DM2_CHANNELS];
    Dm2Paula paula[DM2_CHANNELS];

    uint32_t loop_count;
    uint32_t wrapped_mask; // channels whose track list restarted since the last loop

    FILE* dump_state; // per-tick voice state, for A/B against the original; null when off
    uint32_t tick_number;
};

// True when the data carries the Delta Music 2 ".FNL" mark.
bool dm2_probe(const uint8_t* data, size_t size);

Dm2Module* dm2_create(const uint8_t* data, size_t size);
void dm2_destroy(Dm2Module* module);

// Write per-tick channel state to `path` ("-" = stderr, "" = off).
bool dm2_set_dump_file(Dm2Module* module, const char* path);

int dm2_subsong_count(const Dm2Module* module);
// (Re)start a subsong from its beginning; the module plays through `voices`.
bool dm2_start(Dm2Module* module, int subsong, NuadeVoices* voices);

// Run one player tick and hand the result to the voices. True when the song passed its loop point during it.
bool dm2_tick(Dm2Module* module);

// The longest track list, in positions.
int dm2_order_count(const Dm2Module* module);
Dm2VoiceState dm2_voice_state(const Dm2Module* module, int voice);
// The row `row` of the block at position `order` of the track list of `voice`; empty outside the track list.
Dm2Cell dm2_cell(const Dm2Module* module, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
Dm2Module* dm2_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
