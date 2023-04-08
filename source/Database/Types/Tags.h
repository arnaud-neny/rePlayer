#pragma once

#include <Core/Types.h>
#include <string>

namespace rePlayer
{
    class Tag
    {
    public:
        enum Index
        {
#           define TAG(a) kIndex##a,
#               include "Tags.inc"
#           undef TAG
            kNumTags
        };
        enum Type : uint64_t
        {
            kNone = 0,
#           define TAG(a) k##a = 1ull << kIndex##a,
#               include "Tags.inc"
#           undef TAG
        };

    public:
        constexpr Tag() = default;
        constexpr Tag(Type tag);
        constexpr Tag(uint64_t tag);

        constexpr Tag& Set(Tag tag);
        constexpr Tag& Raise(Tag tag);
        constexpr Tag& Lower(Tag tag);
        constexpr Tag& Enable(Tag tag, bool isEnabled);

        constexpr bool IsEnabled(Tag tag) const;
        constexpr bool IsAnyEnabled(Tag tag) const;
        constexpr bool IsEqual(Tag tag, Tag mask) const;

        constexpr bool operator==(Tag other) const;
        constexpr bool operator!=(Tag other) const;

        template <typename T>
        static constexpr const char* const Name(T index);
        std::string ToString() const;

    private:
        Type m_value = kNone;
        static const char* const ms_names[];
    };
}
// namespace rePlayer

#include "Tags.inl.h"