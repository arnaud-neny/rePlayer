// SPDX-License-Identifier: MIT
// Sean Connolly, sample replayer (Surf Ninjas): the state of one song playing.
//
// Field comments give the replayer's variable (its address in the Surf Ninjas module).

#pragma once

#include "sc_internal.h"

// The register blocks the replayer stages and writes are not cleared by its init routine and hold their values from
// tick to tick.

// The replayer's init routine for `subsong`. `first` loads the module's initial register blocks; a restart keeps the
// blocks the replayer left.
void sc2_init(Sco2* s, ScoModule* module, uint32_t subsong, bool first, NuadePaulaChannel* paula);

// One interrupt: the registers go to `paula`, which the caller hands to the voices.
void sc2_tick(Sco2* s, NuadePaulaChannel* paula);

// The glue's SongEnd test: every voice reached the end of its stream.
bool sc2_ended(const Sco2* s);
