#pragma once

#include <Core/Core.h>

namespace rePlayer
{
    enum class eReplay : uint16_t
    {
        Unknown,
#define REPLAY(a, b) a,
#include "replays.inc"
#undef REPLAY
        Count
    };

    struct Replayables
    {
        eReplay replays[size_t(eReplay::Count)] = { eReplay::Unknown };
        eReplay& operator[](size_t index) { return replays[index]; }
        eReplay operator[](size_t index) const { return replays[index]; }
    };

    enum class eExtension : uint16_t
    {
        Unknown,
#define EXTENSION(a) _##a,
#include "extensions.inc"
#undef EXTENSION
        Count
    };
    static_assert(uint16_t(eExtension::Count) <= 512);

    struct MediaType
    {
        eExtension ext : 9{ eExtension::Unknown };
        eReplay replay : 7{ eReplay::Unknown };

        MediaType() = default;
        MediaType(eExtension otherExt, eReplay otherReplay)
            : ext{ otherExt }
            , replay{ otherReplay }
        {}
        MediaType(const char* const otherExt, eReplay otherReplay);

        bool operator!=(MediaType other) const { return ext != other.ext || replay != other.replay; }

        const char* const GetExtension() const;
        const char* const GetReplay() const;

        static const char* const extensionNames[];
        static const size_t extensionLengths[];
        static const char* const replayNames[];
    };
}
// namespace rePlayer