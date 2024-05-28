#pragma once

#include "DatabaseSongsUI.h"

namespace rePlayer
{
    inline void DatabaseSongsUI::DeleteSubsong(SubsongID subsongId)
    {
        m_deletedSubsongs.AddOnce(subsongId);
    }

    inline bool DatabaseSongsUI::HasDeletedSubsongs(SongID songId) const
    {
        return m_deletedSubsongs.FindIf([songId](auto subsongId)
        {
            return subsongId.songId == songId;
        }) != nullptr;
    }

    inline uint32_t DatabaseSongsUI::NumSubsongs() const
    {
        return m_entries.NumItems();
    }

    inline uint32_t DatabaseSongsUI::NumSelectedSubsongs() const
    {
        return m_numSelectedEntries;
    }

    inline std::string DatabaseSongsUI::GetFullpath(Song* song, Array<Artist*>* artists) const
    {
        (void)song;
        (void)artists;
        return {};
    }

    inline void DatabaseSongsUI::InvalidateCache()
    {}
}
// namespace rePlayer