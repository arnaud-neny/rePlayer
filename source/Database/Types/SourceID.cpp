#include "SourceID.h"

namespace rePlayer
{
    const char* const SourceID::sourceNames[] = {
        "Amiga Music Preservation",
        "The Mod Archive",
        "Modland",
        "File Import",
        "High Voltage SID Collection",
        "SNDH - Atari ST YM2149 Archive",
        "Atari SAP Music Archive",
        "ZX-Art"
    };

    static_assert(_countof(SourceID::sourceNames) == SourceID::NumSourceIDs);
}
// namespace rePlayer