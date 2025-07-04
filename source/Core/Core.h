#pragma once
#pragma warning(disable : 4200)
#pragma warning(disable : 26812)

#include <Core/Types.h>
#include <malloc.h>
#include <string.h>
#include <type_traits>
#include <utility>

namespace core
{
    /**
    * Memory
    */
    inline void* Alloc(size_t size, size_t alignment = 0)
    {
        return _aligned_malloc(size, alignment > 0 ? alignment : 16);
    }

    template <typename T>
    inline T* Alloc(size_t size, size_t alignment = alignof(T))
    {
        return reinterpret_cast<T*>(Alloc(size, alignment));
    }

    inline void* Calloc(size_t count, size_t size)
    {
        auto p = Alloc(size * count, 16);
        memset(p, 0, size * count);
        return p;
    }

    inline void* Realloc(void* ptr, size_t size, size_t alignment = 0)
    {
        return _aligned_realloc(ptr, size, alignment > 0 ? alignment : 16);
    }

    template <typename T>
    inline T* Realloc(void* ptr, size_t size)
    {
        return reinterpret_cast<T*>(ReAlloc(ptr, size, alignof(T)));
    }

    inline void Free(const void* ptr)
    {
        _aligned_free(const_cast<void*>(ptr));
    }

    template <typename T1, typename T2>
    inline constexpr T1 AlignUp(T1 size, T2 alignment)
    {
        return T1((size + alignment - 1) & ~(alignment - 1));
    }

    template <typename ReturnType = uint32_t, typename T, uint32_t NumItems>
    inline constexpr ReturnType NumItemsOf(const T (&)[NumItems])
    {
        return ReturnType(NumItems);
    }

    /**
    * Maths
    */

    template <typename T1, typename T2>
    inline constexpr auto Min(T1&& a, T2&& b)
    {
        return std::forward<T1>(a) < std::forward<T2>(b) ? std::forward<T1>(a) : std::forward<T2>(b);
    }

    template<typename T1, typename T2, typename... Ts>
    inline constexpr auto Min(T1&& a, T2&& b, Ts&&... others)
    {
        return Min(Min(std::forward<T1>(a), std::forward<T2>(b)), std::forward<Ts>(others)...);
    }

    template <typename T1, typename T2>
    inline constexpr auto Max(T1&& a, T2&& b)
    {
        return std::forward<T1>(a) > std::forward<T2>(b) ? std::forward<T1>(a) : std::forward<T2>(b);
    }

    template<typename T1, typename T2, typename... Ts>
    inline constexpr auto Max(T1&& a, T2&& b, Ts&&... others)
    {
        return Max(Max(std::forward<T1>(a), std::forward<T2>(b)), std::forward<Ts>(others)...);
    }

    template <typename T1, typename T2, typename T3>
    inline constexpr auto Clamp(T1&& a, T2&& min, T3&& max)
    {
        assert(min <= max);
        return Max(std::forward<T2>(min), Min(std::forward<T3>(max), std::forward<T1>(a)));
    }

    template <typename T1>
    inline constexpr auto Saturate(T1&& a)
    {
        return Max(static_cast<std::remove_reference<T1>::type>(0), Min(static_cast<std::remove_reference<T1>::type>(1), std::forward<T1>(a)));
    }

    /**
    * Compiler
    */

    template <typename T>
    inline constexpr void UnusedArg(T&&)
    {}

    template<typename T, typename... Ts>
    inline constexpr void UnusedArg(T&& a, Ts&&... others)
    {
        UnusedArg(std::forward<T>(a));
        UnusedArg(std::forward<Ts>(others)...);
    }

    template<typename Dtor>
    class [[nodiscard]] Scope
    {
    public:
        template <typename Ctor>
        Scope(Ctor&& ctor, Dtor&& dtor)
            : m_dtor(std::forward<Dtor>(dtor))
        {
            std::forward<Ctor>(ctor)();
        }
        ~Scope()
        {
            m_dtor();
        }

    private:
        Dtor m_dtor;
    };
}
// namespace core