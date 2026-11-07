#include "nuade_scheduler.h"

void nuade_scheduler_reset(NuadeScheduler* scheduler, u32 timebase_hz, u32 sample_rate) {
    scheduler->timebase_hz = timebase_hz;
    scheduler->sample_rate = sample_rate;
    scheduler->remainder = 0;
}

u32 nuade_scheduler_frames(NuadeScheduler* scheduler, u32 length) {
    const u64 total = scheduler->remainder + (u64)length * scheduler->sample_rate;
    scheduler->remainder = total % scheduler->timebase_hz;
    return (u32)(total / scheduler->timebase_hz);
}
