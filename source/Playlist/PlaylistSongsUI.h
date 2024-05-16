#pragma once

#include <Database/DatabaseSongsUI.h>
#include <Playlist/Playlist.h>

namespace rePlayer
{
    class Playlist::SongsUI : public DatabaseSongsUI
    {
    public:
        SongsUI(Array<char>& paths, Window& owner);
        ~SongsUI() override;

        void OnEndUpdate() override;

        std::string GetFullpath(Song* song, Array<Artist*>* artists = nullptr) const override;

        Playlist& GetPlaylist();

    private:
        Array<char>& m_paths;
    };
}
// namespace rePlayer