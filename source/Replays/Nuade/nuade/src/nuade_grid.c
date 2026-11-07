#include "nuade_grid.h"

#include "nuade_tracker.h"

typedef struct GridView {
    const NuadeScan* scan;
    u32 ticks_per_row;
} GridView;

static u32 grid_rows(const NuadeScan* scan, u32 ticks_per_row) {
    return (scan->ticks + ticks_per_row - 1) / ticks_per_row;
}

NuadeTrackerInfoResult nuade_grid_info(const NuadeScan* scan, u32 channels, u32 ticks_per_row) {
    NuadeTrackerInfoResult result = { 0 };
    if (scan == nullptr || ticks_per_row == 0 || scan->ticks == 0) {
        return result;
    }
    result.valid = true;
    result.info.channels = channels;
    result.info.rows = grid_rows(scan, ticks_per_row);
    result.info.column_count = 2;
    result.info.columns[0] = (NuadeColumnDesc) { .name = "Note", .kind = NuadeColumn_Note };
    result.info.columns[1] = (NuadeColumnDesc) { .name = "Ins", .kind = NuadeColumn_Hex, .digits = 2 };
    return result;
}

// The first event whose tick is >= `tick`; events are in tick order.
static u32 first_at_or_after(const NuadeScan* scan, u64 tick) {
    u32 lo = 0;
    u32 hi = scan->count;
    while (lo < hi) {
        const u32 mid = lo + (hi - lo) / 2;
        if (scan->events[mid].tick < tick) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static NuadePatternCell grid_cell(const void* ctx, u32 order, u32 channel, u32 row) {
    (void)order;
    const GridView* view = (const GridView*)ctx;
    const NuadeScan* scan = view->scan;
    const u64 begin = (u64)row * view->ticks_per_row;
    const u64 end = begin + view->ticks_per_row;
    NuadePatternCell cell = nuade_cell_empty();
    for (u32 i = first_at_or_after(scan, begin); i < scan->count && scan->events[i].tick < end; i++) {
        if (scan->events[i].voice == channel) {
            cell.value[0] = scan->events[i].note;
            cell.value[1] = scan->events[i].instrument;
            break;
        }
    }
    return cell;
}

u32 nuade_grid_cells(const NuadeScan* scan, u32 channels, u32 ticks_per_row, const NuadePosition* at, u32 channel,
                     i32 row_lo, i32 row_hi, NuadePatternCell* out, u32 cap) {
    if (scan == nullptr || ticks_per_row == 0 || at->order != 0) {
        return 0;
    }
    const GridView view = { .scan = scan, .ticks_per_row = ticks_per_row };
    return nuade_tracker_fill(at, channels, grid_rows(scan, ticks_per_row), channel, row_lo, row_hi, out, cap,
                              grid_cell, &view);
}
