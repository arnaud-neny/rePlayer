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