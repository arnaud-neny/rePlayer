// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the 4 voice replayer ("MusicMaker-4V"). It drives Paula's four channels directly.

#pragma once

#include "mm_core.h"

void mm4_start(Mm4* e, const MmSong* song, const MmInstruments* inst, NuadeVoices* voices);
MmTickResult mm4_tick(Mm4* e, NuadeVoices* voices);
