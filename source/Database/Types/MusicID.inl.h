#pragma once

#include "MusicID.h"

namespace rePlayer
{
    inline PlaylistID& operator++(PlaylistID& playlistId)
    {
        ++reinterpret_cast<uint32_t&>(playlistId);
        return playlistId;
    }

    inline MusicID::MusicID(SubsongID otherSubsongId, DatabaseID otherDatabaseId)
        : subsongId(otherSubsongId)
        , databaseId(otherDatabaseId)
    {}

    inline constexpr bool MusicID::operator==(MusicID other) const
    {
        return playlistId == other.playlistId;
    }

    inline constexpr bool MusicID::operator<(MusicID other) const
    {
        return playlistId < other.playlistId;
    }
}
// namespace rePlayer