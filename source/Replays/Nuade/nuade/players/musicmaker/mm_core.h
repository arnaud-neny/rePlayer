// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the sequencer shared by both replayers. A track reads events from its patterns, a voice turns
// them into a note (sample, period, volume) and updates the note every tick. What the replayers do with the result
// (Paula registers for 4 voices, software mixing for 8) is in mm4.c and mm8.c.

#pragma once

#include "mm_internal.h"

typedef enum MmStepKind {
    MmStepKind_Idle,
    MmStepKind_PitchScale,
    MmStepKind_Note,
} MmStepKind;

// What the event of a voice leaves for the replayer.
typedef struct MmStep {
    MmStepKind kind;
    uint16_t pitch_scale;
    bool cut;         // the voice was cut because its list had passed its end: everything it plays stops
    bool has_sample;  // false for a glide, which only changes period and volume
    bool stop_dma;    // 4 voices: the note stops the voice's DMA
    bool enable_dma;  // 4 voices: and the tick switches it on again
    bool resume;      // 8 voices: the sample continues where the voice stopped (slot 0x24)
    bool save_resume; // 8 voices: remember where the voice is (the replayer's copy of its position)
    uint32_t sample;  // offset in the instrument block
    uint32_t length;  // bytes before the loop
    uint32_t loop_sample;
    uint32_t loop_length; // bytes
    uint8_t slot;
    uint8_t note2;
    uint16_t period;
    uint8_t volume;
} MmStep;

void mm_core_start(MmCore* c, MmVariant variant, const MmSong* song, const MmInstruments* inst, uint32_t voice_count);

// The track of `voice` for one tick: runs its next event when the row has run out.
MmStep mm_core_step(MmCore* c, uint32_t voice);

// Applies the event's result to the voice and updates its effects: out_period and out_volume are the register values.
void mm_core_update(MmCore* c, uint32_t voice, const MmStep* step);

// A replayer's tempo: what the table phase advances with.
void mm_core_set_tempo(MmCore* c, uint16_t tempo);

// The eagleplayer's SongEnd: true once, when every track the song has has passed the end of its position list.
bool mm_core_poll_song_end(MmCore* c);

// The replayer has started a sample with a repeat on a voice: the note's table loops with it (`bytes` long).
void mm_core_start_table_loop(MmCore* c, uint32_t voice, uint16_t bytes);
