#include "nuade_tracker.h"

#include <math.h>
#include <stdio.h>

NuadePatternCell nuade_cell_empty(void) {
    NuadePatternCell cell;
    for (u32 i = 0; i < NUADE_MAX_COLUMNS; i++) {
        cell.value[i] = NUADE_CELL_EMPTY;
    }
    return cell;
}

u32 nuade_tracker_fill(const NuadePosition* at, u32 channels, u32 rows, u32 channel, i32 row_lo, i32 row_hi,
                       NuadePatternCell* out, u32 cap, NuadeCellFn cell, const void* ctx) {
    u32 n = 0;
    for (i32 row = row_lo; row < row_hi && n < cap; row++, n++) {
        const bool inside = channel < channels && row >= 0 && (u32)row < rows;
        out[n] = inside ? cell(ctx, at->order, channel, (u32)row) : nuade_cell_empty();
    }
    return n;
}

i32 nuade_period_to_note_shifted(u32 period, i32 octave_shift) {
    if (period == 0) {
        return NUADE_CELL_EMPTY;
    }
    const i32 note = 12 - 12 * octave_shift + (i32)lround(12.0 * log2(856.0 / (double)period));
    return note < 0 ? 0 : note;
}

i32 nuade_period_to_note(u32 period) {
    return nuade_period_to_note_shifted(period, 0);
}

void nuade_note_name(char out[8], i32 value, i32 octave_shift) {
    static const char* const names[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    switch (value) {
        case NUADE_NOTE_OFF:
            snprintf(out, 8, "===");
            return;
        case NUADE_NOTE_CUT:
            snprintf(out, 8, "^^^");
            return;
        default:
            break;
    }
    if (value < 0) {
        snprintf(out, 8, "---");
        return;
    }
    snprintf(out, 8, "%s%d", names[value % 12], value / 12 + octave_shift);
}
