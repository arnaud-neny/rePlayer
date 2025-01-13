#pragma once

#include <Database/Database.h>

namespace rePlayer
{
    class PlaylistDatabase : public Database
    {
    public:
        void Reset() override;

        std::string GetFullpath(Song* song, Array<Artist*>* artists = nullptr) const override;

        SourceID AddPath(SourceID::eSourceID id, const std::string& path);
        const char* GetPath(SourceID sourceId) const;

        Status Load(io::File& file, uint32_t version);
        void Save(io::File& file);

    private:
        Array<char> m_paths; // paths are indexed in the sourceID of the song
    };
}
// namespace rePlayer