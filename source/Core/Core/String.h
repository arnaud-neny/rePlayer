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
}
// namespace core