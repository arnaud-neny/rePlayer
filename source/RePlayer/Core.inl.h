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

    template <typename ItemID>
    inline void Core::OnNewProxy(ItemID id)
    {
        if constexpr (std::is_same<ItemID, SongID>::value)
            ms_instance->m_songsStack.items.Add(id);
        else
            ms_instance->m_artistsStack.items.Add(id);
    }
}
// namespace rePlayer