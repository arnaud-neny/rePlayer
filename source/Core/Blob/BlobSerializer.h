#pragma once

#include <Core.h>
#include <Containers/Array.h>
#include <Containers/Span.h>

namespace core::io
{
    class File;
}
// namespace core::io

namespace core
{
    class BlobSerializer
    {
    public:
        BlobSerializer();

        template <typename T>
        void Store(const T& item);
        template <typename T>
        void Store(const T* items, size_t size);

        void Store(const uint8_t* items, size_t size, size_t alignment);

        void Push(size_t patchOffset, size_t numItems = ~size_t(0));
        void Pop();

        Status Save(io::File& file);

        const Span<uint8_t> Buffer();

    private:
        void Commit();

    private:
        uint16_t m_currentPatch = 0;
        bool m_isValid = false;

        struct Patch
        {
            uint16_t offset = 0; // offset in previous patch
            uint16_t position = 0; // global position
            uint16_t alignment = 0;
            uint16_t previousPatch = 0;
            uint16_t numItems = 0;
            bool hasNumItems = false;
            Array<uint8_t> buffer;
        };
        Array<Patch> m_patches;
    };

    template <typename T>
    inline void BlobSerializer::Store(const T& item)
    {
        if constexpr (requires { &item.Store; })
            return item.Store(*this);
        else
            Store(reinterpret_cast<const uint8_t*>(&item), sizeof(T), alignof(T));
    }

    template <typename T>
    inline void BlobSerializer::Store(const T* items, size_t size)
    {
        Store(reinterpret_cast<const uint8_t*>(items), size, alignof(T));
    }
}
// namespace core