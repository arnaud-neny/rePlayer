#pragma once

#include "DatabaseSongsUI.h"

namespace rePlayer
{
    inline uint32_t DatabaseSongsUI::NumSubsongs() const
    {
        return m_entries.NumItems();
    }

    inline uint32_t DatabaseSongsUI::NumSelectedSubsongs() const
    {
        return m_numSelectedEntries;
    }

    inline bool DatabaseSongsUI::SubsongEntry::IsSelected() const
    {
        return external;
    }

    inline void DatabaseSongsUI::SubsongEntry::Select(bool isEnabled)
    {
        external = isEnabled;
    }
}
// namespace rePlayer