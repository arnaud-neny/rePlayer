// Display state: the snapshot ring (position and voice state per tick), and the optional voice meters (peak and scope).

#pragma once

#include "nuade_base.h"
#include <nuade/nuade.h>

typedef struct NuadeViz NuadeViz;

// Snapshots hold `display_voices` voices, the meters `voice_count` (the mixer's).
NuadeViz* nuade_viz_create(u32 voice_count, u32 display_voices);
void nuade_viz_destroy(NuadeViz* viz);

// Forget every snapshot, peak and scope sample.
void nuade_viz_reset(NuadeViz* viz);

// The clock of a player with a row grid (NuadePlayer.grid_ticks) and no position hook: the framework then reports its
// position. ticks_per_row 0 = no such clock.
typedef struct NuadeGridClock {
    u32 ticks_per_row;
    u32 ticks_run; // ticks the player has run; the last one has the scan stamp ticks_run - 1
} NuadeGridClock;

// Store the state at `frame`: the player's position and voice info, plus the mixer's view when `voices` is set (only
// when the display voices are the mixer's). With a
// grid clock the position is Rows mode, order 0, at the row of the tick that ran last, and every voice sits on that
// order and row.
void nuade_viz_record(NuadeViz* viz, u64 frame, const NuadePlayer* player, void* user_data, const NuadeVoices* voices,
                      NuadeGridClock clock);
NuadeSnapshotResult nuade_viz_find(const NuadeViz* viz, u64 frame, NuadeVoiceView* voices, u32 cap);

// Meters cost a copy of every voice's output, so they run only once asked for.
bool nuade_viz_meters_on(const NuadeViz* viz);
void nuade_viz_enable_meters(NuadeViz* viz);
// Zero the peaks and empty the scope.
void nuade_viz_clear_meters(NuadeViz* viz);
// One block of per-voice output (voice_count buffers of `frames`).
void nuade_viz_feed(NuadeViz* viz, float* const* voices, u32 frames);
u32 nuade_viz_vu(NuadeViz* viz, float* out, u32 cap);
u32 nuade_viz_scope(const NuadeViz* viz, u32 voice, float* out, u32 cap);
