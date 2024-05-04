#pragma once

#include <Core.h>

namespace rePlayer
{
    enum class SongID : uint32_t { Invalid = 0 };

    struct SubsongID
    {
        SongID songId = SongID::Invalid;
        uint32_t index = 0;

        constexpr bool IsValid() const;
        constexpr bool IsInvalid() const;

        constexpr uint64_t Value() const;

        constexpr bool operator==(SubsongID other) const;
        constexpr bool operator<(SubsongID other) const;

        constexpr SubsongID() = default;
        constexpr SubsongID(SongID newSongId, uint32_t newIndex);
    };
}
// namespace rePlayer

#include "SubsongID.inl"