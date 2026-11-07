// Tick scheduler: turns tick lengths (in the player's timebase) into whole output frames without drift.

#pragma once

#include "nuade_base.h"

typedef struct NuadeScheduler {
    u32 timebase_hz;
    u32 sample_rate;
    u64 remainder; // output-frame fraction carried between ticks, in 1/timebase_hz frames; always < timebase_hz
} NuadeScheduler;

void nuade_scheduler_reset(NuadeScheduler* scheduler, u32 timebase_hz, u32 sample_rate);

// Output frames until the next tick for a tick of `length` timebase units: floor(length * rate / timebase) with the
// remainder carried, so the long-run average is exact.
u32 nuade_scheduler_frames(NuadeScheduler* scheduler, u32 length);
