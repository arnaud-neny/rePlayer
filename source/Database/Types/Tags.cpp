#include "Tags.h"
#include <bit>

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
}
// namespace rePlayer