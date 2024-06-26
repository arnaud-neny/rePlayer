#pragma once

#include <stdint.h>
#include <ImGui/stb_sprintf.h>
#include <string>

#define STRINGIZE(s) #s
#define TOSTRING(s) STRINGIZE(s)

namespace core
{
    template <uint32_t N1, uint32_t N2>
    inline int32_t sprintf(char(&buf)[N1], const char(&fmt)[N2])
    {
        return strcpy_s(buf, fmt);
    }

    template <uint32_t N, typename... Arguments>
    inline int32_t sprintf(char(&buf)[N], char const* fmt, Arguments... args)
    {
        return stbsp_snprintf(buf, N, fmt, std::forward<Arguments>(args)...);
    }

    template <typename... Arguments>
    inline int32_t sprintf(char *buf, size_t bufSize, char const* fmt, Arguments... args)
    {
        return stbsp_snprintf(buf, int(bufSize), fmt, std::forward<Arguments>(args)...);
    }

    inline std::string ToLower(const std::string& src)
    {
        std::string dst;
        dst.resize(src.size());
        for (size_t i = 0, e = dst.size(); i < e; ++i)
            dst[i] = char(::tolower(src[i]));
        return dst;
    }

    // Convert html entity to utf8 character
    uint32_t ConvertEntity(const char* src, char* dst);
    // Convert all html entities in a string
    std::string ConvertEntities(const std::string& str);

    // we compare without case; but if the two strings match
    // we are falling back to the case comparison
    int32_t CompareStringMixed(const char* dst, const char* src);

    // return the format (B, KB, MB, GB) and divisor for a size
    std::pair<const char*, double> GetSizeFormat(uint64_t size);
}
// namespace core