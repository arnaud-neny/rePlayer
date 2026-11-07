// The row grid: the pattern view of a player whose voices run on their own timing. Internal, not part of include/nuade.
//
// A silent scan (nuade_scan) lists every sample start with its tick. The grid lays a fixed number of ticks per row over
// that list: row r holds the starts of ticks r * ticks_per_row .. (r + 1) * ticks_per_row - 1, one channel per voice,
// so a voice that starts a note every 7 ticks and one that starts one every 12 ticks share one time axis. The song is
// one pattern (order 0) of as many rows as the scan ran ticks. A cell has a Note and an Instrument column and shows the
// first start of its voice within the row; a later start inside the same row is not shown.

#pragma once

#include "nuade_base.h"
#include <nuade/nuade.h>

// Ticks the scan may run for the grid; a longer song shows only its start (the grid then ends where the scan did).
#define NUADE_GRID_MAX_TICKS 360000

// The layout of the grid; `valid` is false for an empty scan or a bad argument.
NuadeTrackerInfoResult nuade_grid_info(const NuadeScan* scan, u32 channels, u32 ticks_per_row);

// tracker_cells for the grid, with the same contract as NuadePlayer.tracker_cells; 0 for a position other than order 0.
u32 nuade_grid_cells(const NuadeScan* scan, u32 channels, u32 ticks_per_row, const NuadePosition* at, u32 channel,
                     i32 row_lo, i32 row_hi, NuadePatternCell* out, u32 cap);
