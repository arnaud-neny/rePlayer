#pragma once

#include "DatabaseSongsUI.h"

namespace rePlayer
{
    inline void DatabaseSongsUI::DeleteSubsong(SubsongID subsongId)
    {
        m_deletedSubsongs.AddOnce(subsongId);
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