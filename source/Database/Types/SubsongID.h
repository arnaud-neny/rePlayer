#pragma once

#include <Core.h>

namespace rePlayer
{
    enum class SongID : uint32_t { Invalid = 0 };

    struct SubsongID
    {
        SongID songId = SongID::Invalid;
        uint16_t index = 0;
        uint16_t external = 0; // use this externally

        constexpr bool IsValid() const;
        constexpr bool IsInvalid() const;

        constexpr uint64_t Value() const;

        constexpr bool operator==(SubsongID other) const;
        constexpr bool operator<(SubsongID other) const;

        constexpr SubsongID() = default;
        constexpr SubsongID(SongID newSongId, uint16_t newIndex);
    };
}
// namespace rePlayer

#include "SubsongID.inl"