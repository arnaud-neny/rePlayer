#pragma once

#include "RePlayer.h"

namespace rePlayer
{
    inline About& Core::GetAbout()
    {
        return *ms_instance->m_about;
    }

    inline Database& Core::GetDatabase(DatabaseID databaseId)
    {
        return *ms_instance->m_db[int32_t(databaseId)];
    }

    inline Deck& Core::GetDeck()
    {
        return *ms_instance->m_deck;
    }

    inline Library& Core::GetLibrary()
    {
        return *ms_instance->m_library;
    }

    inline Playlist& Core::GetPlaylist()
    {
        return *ms_instance->m_playlist;
    }

    inline Replays& Core::GetReplays()
    {
        return *ms_instance->m_replays;
    }

    inline Settings& Core::GetSettings()
    {
        return *ms_instance->m_settings;
    }

    inline SongEditor& Core::GetSongEditor()
    {
        return *ms_instance->m_songEditor;
    }

    inline void Core::Lock()
    {
        ms_instance->m_isLocked++;
    }

    inline void Core::Unlock()
    {
        ms_instance->m_isLocked--;
    }

    inline bool Core::IsLocked()
    {
        return ms_instance->m_isLocked;
    }
}
// namespace rePlayer