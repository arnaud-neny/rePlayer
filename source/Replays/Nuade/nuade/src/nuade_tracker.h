// Helpers for the pattern hooks of a player (NuadePlayer.tracker_info, tracker_cells). Internal, not part of
// include/nuade.

#pragma once

#include "nuade_base.h"
#include <nuade/viz.h>

// A cell with every column empty.
NuadePatternCell nuade_cell_empty(void);

// Reads one cell of the pattern at `order` for the player; rows and channels are already known to be inside it.
typedef NuadePatternCell (*NuadeCellFn)(const void* ctx, uint32_t order, uint32_t channel, uint32_t row);

// The whole tracker_cells contract in one place: writes rows row_lo .. row_hi - 1 of `channel` into `out` (at most
// `cap`), a row outside 0 .. rows - 1 or a channel outside 0 .. channels - 1 as an empty cell, and calls `cell` for
// the others. Returns how many cells it wrote.
u32 nuade_tracker_fill(const NuadePosition* at, u32 channels, u32 rows, u32 channel, i32 row_lo, i32 row_hi,
                       NuadePatternCell* out, u32 cap, NuadeCellFn cell, const void* ctx);

// The semitone from C-0 of an Amiga period (ProTracker: 856 = C-1 = 12), rounded to the nearest note; the finetune
// tables of the trackers all land on their note. -1 for a period of 0.
i32 nuade_period_to_note(u32 period);

// The same for a Note column whose lowest notes sit below C-0: the value is counted from `octave_shift` octaves below
// C-0
// (`NuadeColumnDesc.octave_shift`, negative), so a period lower than the C-0 one still gets a value >= 0.
i32 nuade_period_to_note_shifted(u32 period, i32 octave_shift);
