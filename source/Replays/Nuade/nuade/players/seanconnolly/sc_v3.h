// SPDX-License-Identifier: MIT
// Sean Connolly, sequencer replayer (Reaxion, Soccer Superstars): the state of one song playing.
//
// Field comments give the replayer's variable (its offset in the Reaxion build's variable block).

#pragma once

#include "sc_internal.h"

typedef enum Sco3Wave {
    Sco3Wave_Sample = 0,    // a sample of the sample table, played whole and then its loop
    Sco3Wave_Sequence = 1,  // a sequence of wave forms
    Sco3Wave_Stream = 2,    // a sample played in chunks
    Sco3Wave_Sweep8 = 8,    // a wave form of 8 bytes, swept
    Sco3Wave_Sweep16 = 0x10 // 16 bytes
                            // any other value: 32 bytes
} Sco3Wave;

// The replayer's init routine for `subsong`.
void sc3_init(Sco3* s, ScoModule* module, uint32_t subsong, NuadePaulaChannel* paula);

// One interrupt: the registers go to `paula`, which the caller hands to the voices.
void sc3_tick(Sco3* s, NuadePaulaChannel* paula);

// The glue's SongEnd test: every voice reached the end of its stream.
bool sc3_ended(const Sco3* s);
