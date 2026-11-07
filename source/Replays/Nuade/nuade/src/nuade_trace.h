// Voice event trace: what a tick player's voices do, written in the event vocabulary of UADE's Paula trace so a diff
// tool can compare the two. Pitch is Hz (HZ) and volume is linear (GAIN) here; the diff tool turns
// them into periods and AUDxVOL.

#pragma once

#include "nuade_base.h"
#include "nuade_test.h"
#include <stdio.h>

typedef struct NuadeTrace NuadeTrace;

// Takes ownership of `file`. `voice_count` voices, timestamps in Paula colour clocks at `sample_rate`.
NuadeTrace* nuade_trace_create(FILE* file, u32 voice_count, bool with_bytes);
// Flushes what is pending and closes the file.
void nuade_trace_destroy(NuadeTrace* trace);

// A new run starts: forget everything seen so far.
void nuade_trace_reset(NuadeTrace* trace, u32 sample_rate);

// Feed it the mixer's events.
void nuade_trace_event(NuadeTrace* trace, const NuadeVoiceEvent* event);
