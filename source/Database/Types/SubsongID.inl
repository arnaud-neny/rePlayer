#pragma once

#include "SubsongID.h"
#include <Containers/HashTypes.h>

namespace rePlayer
{
    inline constexpr bool SubsongID::IsValid() const
    {
        return songId != SongID::Invalid;
    }

    inline constexpr bool SubsongID::IsInvalid() const
    {
        return songId == SongID::Invalid;
    }

    inline constexpr uint64_t SubsongID::Value() const
    {
        return (uint64_t(songId) << 32) | index;
    }

    inline constexpr bool SubsongID::operator==(SubsongID other) const
    {
        return songId == other.songId && index == other.index;
    }

    inline constexpr bool SubsongID::operator<(SubsongID other) const
    {
        return Value() < other.Value();
    }

    inline constexpr SubsongID::SubsongID(SongID newSongId, uint32_t newIndex)
        : songId(newSongId)
        , index(newIndex)
    {}

    inline std::size_t SubsongID::operator()(SubsongID const& subsongId) const noexcept
    {
        return subsongId.Value();
    }
}
// namespace rePlayer

namespace core::Hash
{
    template<>
    inline uint32_t Get(const rePlayer::SubsongID& subsongId)
    {
        return Get(subsongId.Value());
    }
}
// namespace core::Hash