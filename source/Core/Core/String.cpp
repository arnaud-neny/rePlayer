#include "String.h"

namespace core
{
    uint32_t ConvertEntity(const char* src, char* dst)
    {
        // convert numerical entity
        if (src[1] == '#')
        {
            uint32_t v = 0;
            if (sscanf_s(src + 2, "%d;", &v) && v != 0)
            {
                *reinterpret_cast<uint8_t*>(dst) = uint8_t(v & 0xff);
                for (v >>= 8; v & 0xff; v >>= 8)
                {
                    dst++;
                    *reinterpret_cast<uint8_t*>(dst) = uint8_t(v & 0xff);
                }
                for (uint32_t i = 3; src[i]; i++)
                {
                    if (src[i] == ';')
                        return i;
                }
            }
        }

        // convert special entity
        static const char* entities[] = { "nbsp", "quot", "amp", "apos", "lt", "gt" };
        static const char ascii[] = { ' ', '"', '&', '\'', '<', '>' };

        for (size_t i = 0; i < _countof(entities); i++)
        {
            auto s = src + 1;
            for (auto c = entities[i];;)
            {
                if (*c == 0)
                {
                    if (*s == ';')
                    {
                        *dst = ascii[i];
                        return static_cast<uint32_t>(c - entities[i] + 1);
                    }
                    break;
                }
                else if (*c == *s)
                {
                    c++;
                    s++;
                }
                else
                    break;
            }
        }
        *dst = '&';
        return 0;
    }

    std::string ConvertEntities(const std::string& str)
    {
        std::string newStr;
        newStr.resize(str.size());
        auto* src = str.c_str();
        auto* dst = newStr.data();
        for (; *src; src++, dst++)
        {
            auto current = *src;
            if (current == '&')
                src += ConvertEntity(src, dst);
            else
                *dst = current;
        }
        newStr.resize(dst - newStr.data());
        return newStr;
/*
        static const char* entities[] = { "nbsp", "quot", "amp", "apos", "lt", "gt"};
        static const char ascii[] = { ' ', '"', '&', '\'', '<', '>'};

        std::string newStr;
        newStr.resize(str.size());
        auto* src = str.c_str();
        auto* dst = newStr.data();
        for (;*src; src++, dst++)
        {
            auto current = *dst = *src;
            if (current == '&')
            {
                if (src[1] == '#')
                {
                    uint32_t v = 0;
                    if (sscanf_s(src, "&#%d;", &v) && v != 0)
                    {
                        *reinterpret_cast<uint8_t*>(dst) = uint8_t(v & 0xff);
                        for (v >>= 8; v & 0xff; v >>= 8)
                        {
                            dst++;
                            *reinterpret_cast<uint8_t*>(dst) = uint8_t(v & 0xff);
                        }
                        while (*++src != ';');
                    }
                }
                else for (size_t i = 0; i < _countof(entities); i++)
                {
                    auto s = src + 1;
                    for (auto c = entities[i];;)
                    {
                        if (*c == 0)
                        {
                            if (*s == ';')
                            {
                                *dst = ascii[i];
                                src += c - entities[i] + 1;
                                i = _countof(entities);
                            }
                            break;
                        }
                        else if (*c == *s)
                        {
                            c++;
                            s++;
                        }
                        else
                            break;
                    }
                }
            }
        }
        newStr.resize(dst - newStr.data());
        return newStr;
*/
    }

    // we compare without case; but if the two strings match
    // we are falling back to the case comparison
    int32_t CompareStringMixed(const char* dst, const char* src)
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