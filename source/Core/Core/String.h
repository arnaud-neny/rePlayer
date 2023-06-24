#pragma once

#include <stdint.h>
#include <ImGui/stb_sprintf.h>
#include <string>

namespace core
{
    template <uint32_t N, typename... Arguments>
    inline int32_t sprintf(char(&buf)[N], char const* fmt, Arguments... args)
    {
        return stbsp_snprintf(buf, N, fmt, std::forward<Arguments>(args)...);
    }

    inline std::string ToLower(const std::string& src)
    {
        std::string dst;
        dst.resize(src.size());
        for (size_t i = 0, e = dst.size(); i < e; ++i)
            dst[i] = char(::tolower(src[i]));
        return dst;
    }

    // we compare without case; but if the two strings match
    // we are falling back to the case comparison
    inline int32_t CompareStringMixed(const char* dst, const char* src)
    {
        int32_t f0, f1, l0, l1;

        do
        {
            f0 = f1 = uint8_t(*(dst++));
            l0 = l1 = uint8_t(*(src++));
        } while (f0 && (f0 == l0));

        if (f0 || l0)
        {
            for (;;)
            {
                if (f1 >= 'A' && f1 <= 'Z')
                    f1 -= 'A' - 'a';
                if (l1 >= 'A' && l1 <= 'Z')
                    l1 -= 'A' - 'a';
                if (f1 && (f1 == l1))
                {
                    f1 = uint8_t(*(dst++));
                    l1 = uint8_t(*(src++));
                }
                else if (f1 - l1)
                    return f1 - l1;
                else
                    break;
            }
        }
        return f0 - l0;
    }
}
// namespace core