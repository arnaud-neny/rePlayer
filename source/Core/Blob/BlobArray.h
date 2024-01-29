#pragma once

#include <Blob/Blob.h>
#include <Blob/BlobSerializer.h>
#include <Containers/Array.h>
#include <Containers/Span.h>

namespace core
{
    /*
    * Blob::kIsStatic: handle is an offset from this to a number of items (uint16_t) then an buffer of items
    * Blob::kIsDynamic: handle is an Array
    */
    template <typename ItemType, Blob::Storage storage>
    class BlobArray
    {
        struct StaticInfo
        {
            union
            {
                uint16_t value = 0;
                struct
                {
                    uint16_t offset : 13;
                    uint16_t numItems : 3;
                };
            };

            uint16_t NumItems() const;
            ItemType* Items() const;

            const ItemType& operator[](size_t index) const;
        };
        typedef std::conditional<storage == Blob::kIsStatic, StaticInfo, Array<ItemType>>::type Type;
        template <typename ItemType, Blob::Storage otherStorage>
        friend class BlobArray;
    public:
        // Setup
        BlobArray() = default;
        BlobArray(const BlobArray<ItemType, Blob::kIsStatic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobArray(const BlobArray<ItemType, Blob::kIsDynamic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobArray(const BlobArray<ItemType, Blob::kIsStatic>& otherBlobArray) requires (storage == Blob::kIsDynamic);
        BlobArray(const BlobArray<ItemType, Blob::kIsDynamic>& otherBlobArray) requires (storage == Blob::kIsDynamic);
        ~BlobArray();

        // States
        template <typename T = uint32_t>
        T NumItems() const;

        bool IsEmpty() const;
        bool IsNotEmpty() const;

        template <typename OtherItemType, Blob::Storage otherStorage>
        bool operator==(const BlobArray<OtherItemType, otherStorage>& otherArray) const;
        bool operator!=(const Span<ItemType>& otherSpan) const;

        template <typename SearchType>
        const ItemType* Find(const SearchType& searchItem) const;

        // Accessors
        ItemType& operator[](size_t index) requires (storage == Blob::kIsDynamic);
        const ItemType& operator[](size_t index) const;

        operator const Span<ItemType>() const;

        ItemType* Items() const;

        // Modifiers
        BlobArray& operator=(const BlobArray<ItemType, Blob::kIsStatic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobArray& operator=(const BlobArray<ItemType, Blob::kIsDynamic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobArray& operator=(const BlobArray<ItemType, Blob::kIsStatic>& otherBlobArray) requires (storage == Blob::kIsDynamic);
        BlobArray& operator=(const BlobArray<ItemType, Blob::kIsDynamic>& otherBlobArray) requires (storage == Blob::kIsDynamic);
        BlobArray& operator=(const Span<ItemType>& otherSpan) requires (storage == Blob::kIsDynamic);

        ItemType* Push() requires (storage == Blob::kIsDynamic);
        ItemType* Add(const ItemType& otherItem) requires (storage == Blob::kIsDynamic);
        ItemType* Add(ItemType&& otherItem) requires (storage == Blob::kIsDynamic);
        ItemType* Add(const ItemType* otherItems, size_t numOtherItems) requires (storage == Blob::kIsDynamic);

        ItemType* Insert(size_t index, const ItemType& otherItem) requires (storage == Blob::kIsDynamic);
        ItemType* Insert(size_t index, ItemType&& otherItem) requires (storage == Blob::kIsDynamic);

        void RemoveAt(size_t index, size_t numItemsToRemove = 1) requires (storage == Blob::kIsDynamic);
        template <typename SearchType>
        int64_t Remove(const SearchType& item, size_t index = 0) requires (storage == Blob::kIsDynamic);

        Type& Container() requires (storage == Blob::kIsDynamic);

        // Storage
        BlobArray& Clear() requires (storage == Blob::kIsDynamic);
        BlobArray& Resize(size_t numItems) requires (storage == Blob::kIsDynamic);

        // Stl
        const ItemType* begin() const;
        const ItemType* end() const;

        // Serialization
        void Store(BlobSerializer& s) const requires (storage == Blob::kIsDynamic);

    private:
        Type m_handle = {};
    };
}
// namespace core

#include "BlobArray.inl.h"