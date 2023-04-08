#pragma once

#include <Core.h>

namespace rePlayer
{
    #define SongsPath "songs/"
    #define MusicPath "db/"
    #define MusicExt ".rePlayer"

    static constexpr uint32_t kMusicFileStamp = (static_cast<uint32_t>('r') << 0) | (static_cast<uint32_t>('p') << 8) | (static_cast<uint32_t>('l') << 16) | (static_cast<uint32_t>('r') << 24);
}
// namespace rePlayer