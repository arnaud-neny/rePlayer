#pragma once

#include <Core.h>

namespace rePlayer
{
    enum class SongID : uint32_t { Invalid = 0 };

    struct SubsongID
    {
        union
        {
            uint32_t value = 0;
            struct
            {
                uint32_t index : 8;
                SongID songId : 24;
            };
        };

        bool IsValid() const;
        bool IsInvalid() const;

        constexpr bool operator==(SubsongID other) const;
        constexpr bool operator<(SubsongID other) const;

        constexpr SubsongID() = default;
        constexpr SubsongID(SongID newSongId, uint16_t newIndex);

        std::size_t operator()(SubsongID const& subsongId) const noexcept;
    };
}
// namespace rePlayer

#include "SubsongID.inl"