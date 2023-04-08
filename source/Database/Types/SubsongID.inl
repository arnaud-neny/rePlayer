#pragma once

#include "SubsongID.h"
#include <Containers/HashTypes.h>

namespace rePlayer
{
    inline bool SubsongID::IsValid() const
    {
        return value != 0;
    }

    inline bool SubsongID::IsInvalid() const
    {
        return value == 0;
    }

    inline constexpr bool SubsongID::operator==(SubsongID other) const
    {
        return value == other.value;
    }

    inline constexpr bool SubsongID::operator<(SubsongID other) const
    {
        return value < other.value;
    }

    inline constexpr SubsongID::SubsongID(SongID newSongId, uint16_t newIndex)
        : songId(newSongId)
        , index(newIndex)
    {}

    inline std::size_t SubsongID::operator()(SubsongID const& subsongId) const noexcept
    {
        return subsongId.value;
    }
}
// namespace rePlayer

namespace core::Hash
{
    template<>
    inline uint32_t Get(const rePlayer::SubsongID& subsongId)
    {
        return subsongId.value;
    }
}
// namespace core::Hash