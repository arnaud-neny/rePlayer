#pragma once

#include <Database/DatabaseSongsUI.h>
#include <Playlist/Playlist.h>

namespace rePlayer
{
    class Playlist::SongsUI : public DatabaseSongsUI
    {
    public:
        SongsUI(Window& owner);
        ~SongsUI() override;

        void OnEndUpdate() override;

        Playlist& GetPlaylist();
    };
}
// namespace rePlayer