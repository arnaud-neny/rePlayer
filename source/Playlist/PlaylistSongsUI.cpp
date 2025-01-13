#include "PlaylistSongsUI.h"

#include <Core/Log.h>

#include <Database/Database.h>

namespace rePlayer
{
    Playlist::SongsUI::SongsUI(Window& owner)
        : DatabaseSongsUI(DatabaseID::kPlaylist, owner)
    {}

    Playlist::SongsUI::~SongsUI()
    {}

    void Playlist::SongsUI::OnEndUpdate()
    {
        DatabaseSongsUI::OnEndUpdate();

        if (m_deletedSubsongs.IsNotEmpty())
        {
            for (auto subsongIdToRemove : m_deletedSubsongs)
            {
                SmartPtr<Song> holdSong = m_db[subsongIdToRemove.songId];
                auto song = holdSong->Edit();
                Log::Message("Discard: ID_%06X_%02XP \"[%s]%s\"\n", uint32_t(subsongIdToRemove.songId), uint32_t(subsongIdToRemove.index), song->type.GetExtension(), m_db.GetTitleAndArtists(subsongIdToRemove).c_str());

                uint32_t numSubsongs = song->lastSubsongIndex + 1ul;
                song->subsongs[subsongIdToRemove.index].isDiscarded = true;
                for (uint32_t i = 0, e = numSubsongs; i < e; i++)
                {
                    if (song->subsongs[i].isDiscarded)
                        numSubsongs--;
                }
                if (numSubsongs == 0)
                {
                    for (auto artistId : song->artistIds)
                    {
                        auto* artist = m_db[artistId]->Edit();
                        if (--artist->numSongs == 0)
                            m_db.RemoveArtist(artistId);
                    }
                    m_db.RemoveSong(song->id);

                    m_db.Raise(Database::Flag::kSaveArtists);
                }

                GetPlaylist().Discard(MusicID(subsongIdToRemove, DatabaseID::kPlaylist));
            }
            m_db.Raise(Database::Flag::kSaveSongs);
            m_deletedSubsongs.Clear();
        }
    }

    Playlist& Playlist::SongsUI::GetPlaylist()
    {
        return reinterpret_cast<Playlist&>(m_owner);
    }
}
// namespace rePlayer