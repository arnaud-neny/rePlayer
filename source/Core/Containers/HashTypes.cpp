#include "HashTypes.h"

#include <string.h>

namespace core::Hash
{
    template <>
    uint32_t Get(const char* const& data)
    {
        return Get(data, (uint32_t)strlen(data));
    }

    // fast hash function, from http://www.azillionmonkeys.com/qed/hash.html
    uint32_t Get(const void* data, size_t size, uint32_t seed)
    {
        if (size <= 0 || data == nullptr)
            return 0;

        auto buffer = reinterpret_cast<const char*>(data);
        uint32_t hash = seed;
        uint32_t rem = size & 3;

        auto Get16Bits = [](const char* pChar) { return *reinterpret_cast<const uint16_t*>(pChar); };

        // Main loop
        for (size >>= 2; size > 0; --size)
        {
            hash += Get16Bits(buffer);
            const uint32_t tmp = (Get16Bits(buffer + 2) << 11) ^ hash;
            hash = (hash << 16) ^ tmp;
            buffer += 2 * sizeof(uint16_t);
            hash += hash >> 11;
        }

        // Handle end cases
        switch (rem)
        {
        case 3:
            hash += Get16Bits(buffer);
            hash ^= hash << 16;
            hash ^= buffer[sizeof(uint16_t)] << 18;
            hash += hash >> 11;
            break;
        case 2:
            hash += Get16Bits(buffer);
            hash ^= hash << 11;
            hash += hash >> 17;
            break;
        case 1:
            hash += *buffer;
            hash ^= hash << 10;
            hash += hash >> 1;
        }

        // Force "avalanching" of final 127 bits
        hash ^= hash << 3;
        hash += hash >> 5;
        hash ^= hash << 4;
        hash += hash >> 17;
        hash ^= hash << 25;
        hash += hash >> 6;

        return hash;
    }

    template <>
    bool Compare(const char* const& a, const char* const& b)
    {
        return strcmp(a, b) == 0;
    }
}
// namespace core::Hash