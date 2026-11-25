#pragma once

#include "Playlist.h"

// Core
#include <IO/File.h>

// rePlayer
#include <PlayList/PlaylistDatabase.h>
#include <RePlayer/Core.h>

namespace rePlayer
{
    template <typename Predicate>
    inline void Playlist::ForEachPlaylist(Predicate predicate)
    {
        for (auto& summary : m_playlists)
        {
            auto file = io::File::OpenForRead(GetPlaylistFilename(summary.name).c_str());
            if (file.IsValid())
            {
                auto stamp = file.Read<uint32_t>();
                auto version = file.Read<uint32_t>();
                if (stamp == kMusicFileStamp && version <= Core::GetVersion())
                {
                    PlaylistDatabase db;
                    Cue cue(db);
                    cue.entries.Resize(summary.numSubsongs);

                    LoadPlaylist(file, cue, version);

                    predicate(db);

                    file = {};
                    file = io::File::OpenForWrite(GetPlaylistFilename(summary.name).c_str());
                    if (file.IsValid())
                    {
                        file.Write(kMusicFileStamp);
                        file.Write(Core::GetVersion());
                        SavePlaylist(file, cue);
                    }
                }
            }
        }
    }
}
// namespace rePlayer