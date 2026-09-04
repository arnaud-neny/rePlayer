#include <Core/String.h>

#include "Tags.h"

#include <bit>
#include <algorithm>

namespace rePlayer
{
    const char* const Tag::ms_names[] = {
#       define TAG(a) #a,
#           include "Tags.inc"
#       undef TAG
    };

    std::string Tag::ToString() const
    {
        std::string str;
        auto value = uint64_t(m_value);
        while (value != kNone)
        {
            auto index = std::countr_zero(value);
            if (!str.empty())
                str += ',';
            str += ms_names[index];
            value &= ~(1ull << index);
        }
        return str;
    }

    const uint8_t* Tag::BuildSortedIndices()
    {
        static uint8_t indices[kNumTags];

        for (uint8_t i = 0; i < kNumTags; ++i)
            indices[i] = i;
        std::sort(indices, indices + kNumTags, [](uint8_t l, uint8_t r)
        {
            return core::CompareStringMixedLogical(ms_names[l], ms_names[r]) < 0;
        });

        return indices;
    }
}
// namespace rePlayer