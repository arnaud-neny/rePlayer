// Core
#include <Core/Log.h>

// rePlayer
#include <Playlist/Playlist.h>
#include <RePlayer/Core.h>

#include "PlaylistDatabase.h"

namespace rePlayer
{
    void PlaylistDatabase::Reset()
    {
        m_paths = {};
        Database::Reset();
    }

    std::string PlaylistDatabase::GetFullpath(Song* song, Array<Artist*>* artists) const
    {
        (void)artists;
        return GetPath(song->GetSourceId(0));
    }

    void PlaylistDatabase::Update()
    {
        if (m_deletedSubsongs.IsNotEmpty())
        {
            for (auto subsongIdToRemove : m_deletedSubsongs)
            {
                SmartPtr<Song> holdSong = (*this)[subsongIdToRemove.songId];
                auto song = holdSong->Edit();
                Log::Message("Discard: ID_%06X_%02XP \"[%s]%s\"\n", uint32_t(subsongIdToRemove.songId), uint32_t(subsongIdToRemove.index), song->type.GetExtension(), GetTitleAndArtists(subsongIdToRemove).c_str());

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
                        auto* artist = (*this)[artistId]->Edit();
                        if (--artist->numSongs == 0)
                            RemoveArtist(artistId);
                    }
                    RemoveSong(song->id);

                    Raise(Database::Flag::kSaveArtists);
                }

                Core::GetPlaylist().Discard(MusicID(subsongIdToRemove, DatabaseID::kPlaylist));
            }
            Raise(Database::Flag::kSaveSongs);
            m_deletedSubsongs.Clear();
        }
    }

    SourceID PlaylistDatabase::AddPath(SourceID::eSourceID id, const std::string& path)
    {
        auto sourceId = SourceID(id, m_paths.NumItems());
        m_paths.Add(path.c_str(), uint32_t(path.size() + 1));
        return sourceId;
    }

    const char* PlaylistDatabase::GetPath(SourceID sourceId) const
    {
        return m_paths.Items(sourceId.internalId);
    }

    Status PlaylistDatabase::Load(io::File& file, uint32_t version)
    {
        auto status = LoadSongs(file);
        if (status == Status::kOk)
        {
            status = LoadArtists(file);
            if (status == Status::kOk)
            {
                file.Read<uint32_t>(m_paths);
                if (version < 2)
                {
                    for (auto* song : Songs())
                    {
                        if (m_paths[song->GetSourceId(0).internalId] == 0)
                        {
                            auto* songEdit = song->Edit();
                            songEdit->sourceIds[0].internalId++;
                        }
                    }
                }
            }
        }
        if (status == Status::kFail)
        {
            Reset();
            Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
        }
        return status;
    }

    void PlaylistDatabase::Save(io::File& file)
    {
        SaveSongs(file);
        SaveArtists(file);
        file.Write<uint32_t>(m_paths);
    }
}
// namespace rePlayer