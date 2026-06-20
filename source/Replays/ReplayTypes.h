#pragma once

#include <Core/Core.h>

namespace rePlayer
{
    enum class eReplay : uint32_t
    {
        Unknown,
#define REPLAY(a, b) a,
#include "replays.inc"
#undef REPLAY
        Count
    };
    static_assert(uint16_t(eReplay::Count) <= 64);

    struct Replayables
    {
        eReplay replays[size_t(eReplay::Count)] = { eReplay::Unknown };
        eReplay& operator[](size_t index) { return replays[index]; }
        eReplay operator[](size_t index) const { return replays[index]; }
    };

    enum class eExtension : uint32_t
    {
        Unknown,
#define EXTENSION(a) _##a,
#define NO_EXTENSION() _,
#include "extensions.inc"
#undef NO_EXTENSION
#undef EXTENSION
        Count
    };
    static_assert(uint16_t(eExtension::Count) <= 2048);

    struct MediaType
    {
        union
        {
            uint32_t value = 0;
            struct
            {
                eExtension ext : 11;
                eReplay replay : 6;
                uint32_t dummy : 15;
            };
        };

        MediaType() = default;
        MediaType(eExtension otherExt, eReplay otherReplay)
            : ext(otherExt)
            , replay(otherReplay)
            , dummy(0)
        {}
        MediaType(const char* const otherExt, eReplay otherReplay);

        bool operator==(MediaType other) const { return value == other.value; }
        bool operator!=(MediaType other) const { return value != other.value; }

        template <typename T = char>
        const T* const GetExtension() const;
        const char* const GetReplay() const;

        static const char* const extensionNames[];
        static const size_t extensionLengths[];
        static const char* const replayNames[];

        static const char* sortedExtensionNames[];
        static eExtension sortedExtensions[];
        static int32_t mapSortedExtensions[];
    };

    template <typename T>
    inline const T* const MediaType::GetExtension() const
    {
        return reinterpret_cast<const T*>(extensionNames[int(ext)]);
    }
}
// namespace rePlayer