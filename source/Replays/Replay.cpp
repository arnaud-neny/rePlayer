#include "Replay.h"

namespace rePlayer
{
    #define EXTENSION(a) #a,
    const char* const MediaType::extensionNames[] = {
        "---",
        #include "Extensions.inc"
    };
    #undef EXTENSION

    #define EXTENSION(a) sizeof(#a) - 1,
    const size_t MediaType::extensionLengths[] = {
        sizeof("---") - 1,
        #include "Extensions.inc"
    };
    #undef EXTENSION

    #define REPLAY(a, b) #a,
    const char* const MediaType::replayNames[] = {
        "Dummy",
        #include "Replays.inc"
    };
    #undef REPLAY

    MediaType::MediaType(const char* const otherExt, eReplay otherReplay)
        : replay{ otherReplay }
        , ext{ eExtension::Unknown }
    {
        for (auto& extensionName : extensionNames)
        {
            if (_stricmp(extensionName, otherExt) == 0)
            {
                ext = static_cast<eExtension>(&extensionName - extensionNames);
                break;
            }
        }
    }

    const char* const MediaType::GetExtension() const
    {
        return extensionNames[static_cast<int32_t>(ext)];
    }

    const char* const MediaType::GetReplay() const
    {
        return replayNames[static_cast<int32_t>(replay)];
    }
}
// namespace rePlayer