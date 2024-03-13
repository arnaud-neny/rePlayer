#pragma once

#include <Core.h>

namespace rePlayer
{
    struct SourceID
    {
        enum eSourceID : uint32_t
        {
            AmigaMusicPreservationSourceID  = 0,
            TheModArchiveSourceID           = 1,
            ModlandSourceID                 = 2,
            FileImportID                    = 3,
            HighVoltageSIDCollectionID      = 4,
            SNDHID                          = 5,
            AtariSAPMusicArchiveID          = 6,
            ZXArtID                         = 7,
            URLImportID                     = 8,
            VGMRipsID                       = 9,

            NumSourceIDs
        };

        SourceID() = default;
        constexpr SourceID(eSourceID newSourceId, uint32_t newInternalId) : sourceId(newSourceId), internalId(newInternalId) {}

        constexpr bool operator==(SourceID other) const { return sourceId == other.sourceId && internalId == other.internalId; }

        union
        {
            uint32_t value = 0;
            struct
            {
                eSourceID sourceId : 8;
                uint32_t internalId : 24;
            };
        };

        static const char* const sourceNames[];
    };

    static constexpr SourceID kInvalidSourceID = { SourceID::AmigaMusicPreservationSourceID, 0 };
}
// namespace rePlayer