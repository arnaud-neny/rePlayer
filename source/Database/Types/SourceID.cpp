#include "SourceID.h"

#include <stdlib.h>

namespace rePlayer
{
    const char* const SourceID::sourceNames[] = {
        "Amiga Music Preservation",
        "The Mod Archive",
        "Modland",
        "File",
        "High Voltage SID Collection",
        "SNDH - Atari ST YM2149 Archive",
        "Atari SAP Music Archive",
        "ZX-Art",
        "URL",
        "VGMRips"
    };
    static_assert(_countof(SourceID::sourceNames) == SourceID::NumSourceIDs);

    const int32_t SourceID::sourceProrities[] = {
        65536,      // AmigaMusicPreservationSourceID
        65537,      // TheModArchiveSourceID
        65538,      // ModlandSourceID
        0x7fFFffFF, // FileImportID
        0,          // HighVoltageSIDCollectionID
        1,          // SNDHID
        2,          // AtariSAPMusicArchiveID
        3,          // ZXArtID
        -1,         // UrlImportID
        4           // VGMRips
    };
    static_assert(_countof(SourceID::sourceProrities) == SourceID::NumSourceIDs);
}
// namespace rePlayer