#include "Replay.h"
#include "ReplayPlugin.h"

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

    const char* const MediaType::GetReplay() const
    {
        return replayNames[static_cast<int32_t>(replay)];
    }

    extern ReplayPlugin g_replayPlugin;

    const uint32_t Replay::Patterns::colors[Patterns::kNumColors] = {
        0xffa0a0a0, // Default
        0xffFFffFF, // Text
        0xff808080, // Empty
        0xff80FF80, // Instrument
        0xffFF8080, // Volume
        0xff40a0a0, // Pitch
        0xff8080FF  // Global
    };

    Replay::~Replay()
    {
        g_replayPlugin.onDelete(this);
    }
}
// namespace rePlayer