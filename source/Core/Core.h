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

    // - auto scopeState = Scope([](){ do some stuff on constructor }, [](){ do some stuff on destructor });
    // - auto scopeData = Scope([](){ return data on constructor }, [](data){ do some stuff on destructor with data });
    template<typename Ctor, typename Dtor>
    class [[nodiscard]] Scope
    {
        using CtorDataType = std::invoke_result_t<Ctor&>;
        static constexpr bool is_void = std::is_void_v<CtorDataType>;
        using DataType = std::conditional_t<is_void, int, CtorDataType>;
        static constexpr bool is_ptr = std::is_pointer_v<DataType>;
        using DataPointerType = std::conditional_t<is_ptr, DataType, DataType*>;

        struct StorageDtor
        {
            Dtor m_dtor;
        };
        struct StorageDtorData
        {
            Dtor m_dtor;
            DataType m_data;
        };
        using Storage = std::conditional_t<is_void, StorageDtor, StorageDtorData>;

    public:
        Scope(Ctor&& ctor, Dtor&& dtor)
            : m_storage{ .m_dtor = std::forward<Dtor>(dtor) }
        {
            if constexpr (!is_void)
                m_storage.m_data = std::forward<Ctor>(ctor)();
            else
                std::forward<Ctor>(ctor)();
        }

        ~Scope()
        {
            if constexpr (!is_void)
                m_storage.m_dtor(m_storage.m_data);
            else
                m_storage.m_dtor();
        }

        operator DataType& () requires (!is_void) { return m_storage.m_data; }
        operator const DataType& () const requires (!is_void) { return m_storage.m_data; }
        DataPointerType operator->() requires (!is_void)
        {
            if constexpr (is_ptr)
                return m_storage.m_data;
            else
                return &m_storage.m_data;
        }

        template <typename OnDetach>
        DataType Detach(OnDetach&& onDetach) requires (!is_void)
        {
            DataType data = m_storage.m_data;
            m_storage.m_data = std::forward<OnDetach>(onDetach)();
            return data;
        }

        DataType Detach() requires (!is_void)
        {
            DataType data = m_storage.m_data;
            m_storage.m_data = {};
            return data;
        }

    private:
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;

    private:
        Storage m_storage;
    };
}
// namespace core