#pragma once

#include "HashTypes.h"

namespace core::Hash
{
    template<>
    inline uint32_t Get(const uint8_t& data)
    {
        return uint32_t(data);
    }

    template<>
    inline uint32_t Get(const int8_t& data)
    {
        return uint32_t(data);
    }

    template<>
    inline uint32_t Get(const uint16_t& data)
    {
        return uint32_t(data);
    }

    template<>
    inline uint32_t Get(const int16_t& data)
    {
        return uint32_t(data);
    }

    template<>
    inline uint32_t Get(const uint32_t& data)
    {
        return data;
    }

    template<>
    inline uint32_t Get(const int32_t& data)
    {
        return uint32_t(data);
    }

    template<>
    inline uint32_t Get(const uint64_t& data)
    {
        return uint32_t(data) + uint32_t(data >> 32) * 23;
//         const uint64_t t = data * 0x9e3779b97f4a7c19ull;
//         return uint32_t(t ^ (t >> 32));
    }

    template<>
    inline uint32_t Get(const int64_t& data)
    {
        return uint32_t(data) + uint32_t(data >> 32) * 23;
//         GetHash(static_cast<uint64_t>(data));
    }

    template<>
    inline uint32_t Get(const float& data)
    {
        return *reinterpret_cast<const uint32_t*>(&data);
    }

    template<>
    inline uint32_t Get(const double& data)
    {
        return Get(*reinterpret_cast<const uint64_t*>(&data));
    }

//     template<>
//     inline uint32_t Get(const char* const& data)
//     {
//         return Get(data, (uint32_t)strlen(data));
//     }

//     template<>
//     inline uint32_t Get(const ls::stl::string& string)
//     {
//         return Get(string.begin(), (uint32_t)string.length());
//     }

//     template<>
//     inline uint32_t Get(uint128_t data)
//     {
//         uint64_t* dataPtr = reinterpret_cast<uint64_t*>(&data);
//         const uint32_t data1 = uint32_t((dataPtr[0] * 0x9e3779b97f4a7c19ull) >> 32);
//         const uint32_t data2 = uint32_t((dataPtr[1] * 0x9e3779b97f4a7c19ull) >> 32);
//         return data1 ^ data2;
//     }

    template<typename T0, typename T1>
    inline bool Compare(const T0& a, const T1& b)
    {
        return a == b;
    }

//     template<>
//     inline bool Compare(const char* const& a, const char* const& b)
//     {
//         return strcmp(a, b) == 0;
//     }

//     template<>
//     inline bool Compare(const char* const& a, const std::string& b)
//     {
//         return strcmp(a, b.begin()) == 0;
//     }

//     template<>
//     inline bool Compare(const std::string& a, const char* const& b)
//     {
//         return strcmp(a.begin(), b) == 0;
//     }

    //added the float stuff as special cases since 0 == -0 if comparing floats normally which can crash the hashmap when resizing
    template<>
    inline bool Compare(const float& a, const float& b)
    {
        return reinterpret_cast<const uint32_t&>(a) == reinterpret_cast<const uint32_t&>(b);
    }
}
// namespace core::Hash