#pragma once

#include "BlobArray.h"
#include <Blob/BlobSerializer.h>

namespace core
{
    template <typename ItemType, Blob::Storage storage>
    inline uint16_t BlobArray<ItemType, storage>::StaticInfo::NumItems() const
    {
        return numItems < 7 ? numItems : reinterpret_cast<uint16_t*>(uintptr_t(this) + offset)[-1];
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::StaticInfo::Items() const
    {
        auto buf = reinterpret_cast<uintptr_t>(this) + offset;
        return reinterpret_cast<ItemType*>(buf);
    }

    template <typename ItemType, Blob::Storage storage>
    inline const ItemType& BlobArray<ItemType, storage>::StaticInfo::operator[](uint32_t index) const
    {
        return Items()[index];
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>::BlobArray(const BlobArray<ItemType, Blob::kIsStatic>& otherBlobArray) requires (storage == Blob::kIsDynamic)
    {
        m_handle.Add(otherBlobArray.Items(), otherBlobArray.NumItems());
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>::BlobArray(const BlobArray<ItemType, Blob::kIsDynamic>& otherBlobArray) requires (storage == Blob::kIsDynamic)
        : m_handle(otherBlobArray.m_handle)
    {}

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>::~BlobArray()
    {
        if constexpr (storage == Blob::kIsStatic)
        {
            auto items = Items();
            for (uint32_t i = 0, e = NumItems(); i < e; i++)
                items[i].~ItemType();
        }
    }

    template <typename ItemType, Blob::Storage storage>
    template <typename T>
    inline T BlobArray<ItemType, storage>::NumItems() const
    {
        return T(m_handle.NumItems());
    }

    template <typename ItemType, Blob::Storage storage>
    inline bool BlobArray<ItemType, storage>::IsEmpty() const
    {
        return NumItems() == 0;
    }

    template <typename ItemType, Blob::Storage storage>
    inline bool BlobArray<ItemType, storage>::IsNotEmpty() const
    {
        return NumItems() != 0;
    }

    template <typename ItemType, Blob::Storage storage>
    template <typename OtherItemType, Blob::Storage otherStorage>
    inline bool BlobArray<ItemType, storage>::operator==(const BlobArray<OtherItemType, otherStorage>& otherArray) const
    {
        return Span<ItemType>(*this).IsEqual(Span<OtherItemType>(otherArray));
    }

    template <typename ItemType, Blob::Storage storage>
    inline bool BlobArray<ItemType, storage>::operator!=(const Span<ItemType>& otherSpan) const
    {
        return Span<ItemType>(*this) != otherSpan;
    }

    template <typename ItemType, Blob::Storage storage>
    template <typename SearchType>
    inline const ItemType* BlobArray<ItemType, storage>::Find(const SearchType& searchItem) const
    {
        for (auto& item : Span<ItemType>(*this))
        {
            if (item == searchItem)
                return &item;
        }
        return nullptr;
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType& BlobArray<ItemType, storage>::operator[](uint32_t index) requires (storage == Blob::kIsDynamic)
    {
        return m_handle[index];
    }

    template <typename ItemType, Blob::Storage storage>
    inline const ItemType& BlobArray<ItemType, storage>::operator[](uint32_t index) const
    {
        return m_handle[index];
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>::operator const Span<ItemType>() const
    {
        return { m_handle.Items(), m_handle.NumItems() };
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Items() const
    {
        return m_handle.Items();
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>& BlobArray<ItemType, storage>::operator=(const BlobArray<ItemType, Blob::kIsStatic>& otherBlobArray) requires (storage == Blob::kIsDynamic)
    {
        m_handle.Clear();
        m_handle.Add(otherBlobArray.Items(), otherBlobArray.NumItems());
        return *this;
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>& BlobArray<ItemType, storage>::operator=(const BlobArray<ItemType, Blob::kIsDynamic>& otherBlobArray) requires (storage == Blob::kIsDynamic)
    {
        m_handle = otherBlobArray.m_handle;
        return *this;
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>& BlobArray<ItemType, storage>::operator=(const Span<ItemType>& otherSpan) requires (storage == Blob::kIsDynamic)
    {
        m_handle.Clear();
        m_handle.Add(otherSpan.Items(), otherSpan.NumItems());
        return *this;
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Push() requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Push();
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Add(const ItemType& otherItem) requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Add(otherItem);
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Add(ItemType&& otherItem) requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Add(std::move(otherItem));
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Add(const ItemType* otherItems, uint32_t numOtherItems) requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Add(otherItems, numOtherItems);
    }

    template <typename ItemType, Blob::Storage storage>
    inline void BlobArray<ItemType, storage>::RemoveAt(uint32_t index, uint32_t numItemsToRemove) requires (storage == Blob::kIsDynamic)
    {
        m_handle.RemoveAt(index, numItemsToRemove);
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Insert(uint32_t index, const ItemType& otherItem) requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Insert(index, otherItem);
    }

    template <typename ItemType, Blob::Storage storage>
    inline ItemType* BlobArray<ItemType, storage>::Insert(uint32_t index, ItemType&& otherItem) requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Insert(index, std::forward<ItemType>(otherItem));
    }

    template <typename ItemType, Blob::Storage storage>
    template <typename SearchType>
    inline int64_t BlobArray<ItemType, storage>::Remove(const SearchType& searchItem, uint32_t index) requires (storage == Blob::kIsDynamic)
    {
        return m_handle.Remove(searchItem, index);
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>::Type& BlobArray<ItemType, storage>::Container() requires (storage == Blob::kIsDynamic)
    {
        return m_handle;
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>& BlobArray<ItemType, storage>::Clear() requires (storage == Blob::kIsDynamic)
    {
        m_handle.Clear();
        return *this;
    }

    template <typename ItemType, Blob::Storage storage>
    inline BlobArray<ItemType, storage>& BlobArray<ItemType, storage>::Resize(uint32_t numItems) requires (storage == Blob::kIsDynamic)
    {
        m_handle.Resize(numItems);
        return *this;
    }

    template <typename ItemType, Blob::Storage storage>
    inline const ItemType* BlobArray<ItemType, storage>::begin() const
    {
        return Items();
    }

    template <typename ItemType, Blob::Storage storage>
    inline const ItemType* BlobArray<ItemType, storage>::end() const
    {
        const ItemType* items;
        if constexpr (storage == Blob::kIsStatic)
            items = m_handle.Items() + m_handle.NumItems();
        else
            items = m_handle.end();
        return items;
    }

    template <typename ItemType, Blob::Storage storage>
    inline void BlobArray<ItemType, storage>::Store(BlobSerializer& s) const requires (storage == Blob::kIsDynamic)
    {
        StaticInfo info = {};
        info.numItems = m_handle.NumItems() < 7 ? m_handle.NumItems() : 7;
        s.Store(info);
        if (info.numItems != 0)
        {
            if (info.numItems < 7)
                s.Push(sizeof(StaticInfo));
            else
                s.Push(sizeof(StaticInfo), m_handle.NumItems());
            if constexpr (requires { &m_handle[0].Store; })
            {
                for (auto& item : m_handle)
                    item.Store(s);
            }
            else
            {
                s.Store(m_handle.Items(), m_handle.Size<uint32_t>());
            }
            s.Pop();
        }
    }

    template <typename ItemType, Blob::Storage storage>
    inline void BlobArray<ItemType, storage>::Load(io::File& file) requires (storage == Blob::kIsDynamic)
    {
        if constexpr (requires { &m_handle[0].Load; })
        {
            m_handle.Resize(file.Read<uint32_t>());
            for (auto& item : m_handle)
                item.Load(file);
        }
        else
            file.Read<uint32_t>(m_handle);
    }

    template <typename ItemType, Blob::Storage storage>
    inline void BlobArray<ItemType, storage>::Save(io::File& file) const requires (storage == Blob::kIsDynamic)
    {
        if constexpr (requires { &m_handle[0].Save; })
        {
            file.WriteAs<uint32_t>(m_handle.NumItems());
            for (auto& item : m_handle)
                item.Save(file);
        }
        else
            file.Write<uint32_t>(m_handle);
    }
}
// namespace core