#pragma once

#include <Core.h>
#include <vector>

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace Organya
{
    struct Song;

    void initialize();
    void release();
    Song* Load(core::io::Stream* stream, uint32_t sampleRate);
    void Unload(Song* song);
    void Reset(Song* song);
    uint32_t GetDuration(Song* song);
    int32_t GetVersion(Song* song);
    std::vector<float> Render(Song* song);
}
// namespace Organya