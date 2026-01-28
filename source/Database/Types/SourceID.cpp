#include <Core.h>
#include "SourceID.h"

#include <stdlib.h>

namespace rePlayer
{
    using namespace core;

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
    static_assert(NumItemsOf(SourceID::sourceNames) == SourceID::NumSourceIDs);

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
    static_assert(NumItemsOf(SourceID::sourceProrities) == SourceID::NumSourceIDs);

    const int32_t SourceID::playlistProrities[] = {
        0,          // AmigaMusicPreservationSourceID
        1,          // TheModArchiveSourceID
        2,          // ModlandSourceID
        65536,      // FileImportID
        0,          // HighVoltageSIDCollectionID
        3,          // SNDHID (after Modland as content is too old on this site)
        0,          // AtariSAPMusicArchiveID
        0,          // ZXArtID
        65537,      // UrlImportID
        0           // VGMRips
    };
    static_assert(NumItemsOf(SourceID::playlistProrities) == SourceID::NumSourceIDs);
}
// namespace rePlayer