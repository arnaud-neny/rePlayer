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
    static_assert(uint16_t(eExtension::Count) <= 1024);

    struct MediaType
    {
        union
        {
            uint16_t value = 0;
            struct
            {
                eExtension ext : 10;
                eReplay replay : 6;
            };
        };

        MediaType() = default;
        MediaType(eExtension otherExt, eReplay otherReplay)
            : ext{ otherExt }
            , replay{ otherReplay }
        {}
        MediaType(const char* const otherExt, eReplay otherReplay);

        bool operator==(MediaType other) const { return value == other.value; }
        bool operator!=(MediaType other) const { return value != other.value; }

        template <typename T = char>
        const T* const GetExtension() const;
        const char* const GetReplay() const;

        template <typename T = char>
        static const T* const GetExtension(size_t index);

        static const char* const extensionNames[];
        static const size_t extensionLengths[];
        static const char* const replayNames[];
    };

    template <typename T>
    inline const T* const MediaType::GetExtension() const
    {
        return GetExtension<T>(static_cast<size_t>(ext));
    }

    template <typename T>
    inline const T* const MediaType::GetExtension(size_t index)
    {
        return reinterpret_cast<const T* const>(extensionNames[index]);
    }
}
// namespace rePlayer