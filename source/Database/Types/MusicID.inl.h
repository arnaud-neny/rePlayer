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

    inline bool MusicID::operator==(MusicID other) const
    {
        return playlistId == other.playlistId;
    }

    inline uint64_t MusicID::GetId() const
    {
        return (uint64_t(subsongId.value) << 32) | uint64_t(playlistId);
    }
}
// namespace rePlayer