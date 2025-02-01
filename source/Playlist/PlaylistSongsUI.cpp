#include "PlaylistSongsUI.h"

namespace rePlayer
{
    Playlist::SongsUI::SongsUI(Window& owner)
        : DatabaseSongsUI(DatabaseID::kPlaylist, owner)
    {}

    Playlist::SongsUI::~SongsUI()
    {}
}
// namespace rePlayer