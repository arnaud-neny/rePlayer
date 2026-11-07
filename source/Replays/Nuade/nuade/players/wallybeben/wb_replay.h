// SPDX-License-Identifier: MIT
// Wally Beben: the replayer.
//
// One call of the module's tick routine is one wb_replay_tick: a dispatcher that starts a requested song or plays one
// tick, a loop over the four voices, and the output of the Paula registers. The voice records are the original's:
// field f of voice v is byte f + v of the block, so each field is an array of four bytes.

#pragma once

#include "nuade_paula_channel.h"
#include "wb_internal.h"

// Start the replayer on the module: the state of a fresh load, the eagleplayer's InitSound (it requests `song`, 1
// based, and runs the reset routine). `sample_ptr` holds module->sample_count entries.
void wb_replay_init(WbReplay* replay, const WbModule* module, uint32_t song, NuadePaulaChannel* paula,
                    uint32_t* sample_ptr);

// One tick of the module's interrupt routine.
void wb_replay_tick(WbReplay* replay);

// Plays every subsong of the module until it stops or its state repeats, and reports whether the replayer never met
// something it does not model (see wb_replay.c). The ticks are charged to `budget`.
bool wb_replay_prove(const WbModule* module, WbBudget* budget);
