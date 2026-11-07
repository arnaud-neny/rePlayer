// SPDX-License-Identifier: MIT
// Mark II sound system, translated from the UADE eagleplayer "MarkII" (Wanted Team). Every module carries its
// replayer: a 4-voice sequencer of 4-byte steps (a step is two ticks) that the eagleplayer finds by the instructions
// of the code and calls through three entry modes (init, tick, fade).

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

#define MK2_VOICES 4

// The CIA clock: the timebase of MK2_TICK_LENGTH.
#define MK2_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / MK2_TIMEBASE_HZ seconds.
#define MK2_TICK_LENGTH 14187

typedef struct Mk2Player Mk2Player;

typedef struct Mk2TickResult {
    bool looped; // the song ran past its last row: the module's patched code calls SongEnd and starts over
} Mk2TickResult;

typedef struct Mk2VoiceState {
    int period;     // the last period written, 0 if none
    int instrument; // the sample number last started, -1 if none
} Mk2VoiceState;

// A column of the rows. The replayer reads the columns last to first; column 3 starts on Paula channel 0. A step can
// swap a column with the one below it, which moves both to the other's channel. The register address and the DMA bit
// of a column are two separate variables of the replayer.
typedef struct Mk2Track {
    uint8_t voice;     // the channel whose registers the column writes
    uint8_t dma_voice; // the channel whose DMA bit the column's step selects
    bool slide;        // the last step has the slide flag
    uint16_t param;    // the parameter byte of the last step
    uint16_t note;     // its note plus the row's transpose
    uint16_t last_period;
    int16_t envelope_fall;
    int16_t envelope_rise;
} Mk2Track;

typedef struct Mk2Voice {
    NuadePaulaChannel paula;
    bool restart; // the last step asked for the DMA to stop on the next tick
    int instrument;
} Mk2Voice;

// One replayer version: the code the port knows (by hash, outside the variables inside the code) and the module
// offsets of its variables and tables.
typedef struct Mk2Layout {
    uint64_t code_hash; // FNV-1a over the code, the envelope cells left out
    uint32_t code_size;
    bool slides;              // the tick without a new step runs the slide of the steps' parameter byte
    bool holds_note;          // a step with the note 0x32 keeps the voice's period
    bool masks_sample;        // bit 7 of a step's sample byte is a flag, not part of the sample number
    bool signals_song_end;    // the eagleplayer patches the wrap of the song to call SongEnd
    uint32_t envelope;        // 4 falling counters, then 4 rising ones (words), inside the code area
    uint32_t sample_flags;    // the sample byte of each voice's last step (words); slides only
    uint32_t param;           // the parameter byte of each voice's last step (words); slides only
    uint32_t note;            // the note of each voice's last step, transposed (words); slides only
    uint32_t last_period;     // the last period of each voice (words)
    uint32_t toggle;          // word: the next tick is a step (nonzero) or the DMA tick (zero)
    uint32_t sequence_offset; // long: from `data` to the step sequences
    uint32_t sample_offset;   // long: from `data` to the samples
    uint32_t song_rows;       // word
    uint32_t block_size;      // word: bytes of one sequence (4 per step)
    uint32_t instruments;     // 8 bytes each
    uint32_t periods;         // words
    uint32_t data;            // the rows of the song
} Mk2Layout;

typedef struct Mk2Module {
    const uint8_t* data;
    size_t size;
    const Mk2Layout* layout;
} Mk2Module;

// The variables the replayer's init leaves alone: they start as the module has them.
typedef struct Mk2Initial {
    uint16_t envelope_fall[MK2_VOICES];
    uint16_t envelope_rise[MK2_VOICES];
    bool slide[MK2_VOICES]; // bit 7 of the sample byte of the last step
    uint16_t param[MK2_VOICES];
    uint16_t note[MK2_VOICES];
    uint16_t last_period[MK2_VOICES];
    bool toggle;
} Mk2Initial;

typedef struct Mk2Song {
    uint32_t sequence_start; // module offset of the first sequence
    uint32_t sample_start;   // module offset of the first sample
    uint32_t data_start;     // module offset of the first row
    uint16_t rows;
    uint16_t block_size;
} Mk2Song;

struct Mk2Player {
    uint8_t* chip; // the module, padded for the Paula channels
    Mk2Module module;
    Mk2Song song;
    Mk2Initial initial;
    uint32_t row_start;   // module offset of the current row
    uint32_t step_offset; // module offset of the current step in the sequences
    uint16_t row;
    uint16_t step_bytes; // 4 per step done in the sequences
    bool step_next;      // the next tick plays a step (otherwise it is the first tick of a step)
    bool fault;          // the module led out of what the port models
    Mk2Track tracks[MK2_VOICES];
    Mk2Voice voices[MK2_VOICES];
};

// The eagleplayer's Check2 and the replayer version of the module (only code the port knows is accepted), plus what
// this port needs: every table, step and sample the song reaches lies inside the module. Null when one refuses it.
Mk2Player* mk2_create(const uint8_t* data, size_t size);
void mk2_destroy(Mk2Player* player);

// (Re)start the song: the eagleplayer's InitSound, which plays the first step.
bool mk2_start(Mk2Player* player, NuadeVoices* voices);

Mk2TickResult mk2_tick(Mk2Player* player, NuadeVoices* voices);

// Ticks per step.
uint32_t mk2_grid_ticks(const Mk2Player* player);

Mk2VoiceState mk2_voice_state(const Mk2Player* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
Mk2Player* mk2_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
