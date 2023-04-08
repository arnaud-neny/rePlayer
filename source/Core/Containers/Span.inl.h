#pragma once

#include "Span.h"

namespace core
{
    template <typename ItemType>
    inline Span<ItemType>::Span(ItemType* items, size_t numItems)
        : m_items(items)
        , m_numItems(uint32_t(numItems))
    {}

    template <typename ItemType>
    inline Span<ItemType>::Span(ItemType* beginItems, ItemType* endItems)
        : Span(beginItems, endItems - beginItems)
    {}

    template <typename ItemType>
    inline Span<ItemType>::Span(const Span& otherSpan)
        : Span(otherSpan.m_items, otherSpan.m_numItems)
    {}

    template <typename ItemType>
    inline Span<ItemType>::Span(Span&& otherSpan)
        : Span(otherSpan.m_items, otherSpan.m_numItems)
    {
        otherSpan.m_items = nullptr;
        otherSpan.m_numItems = 0;
    }

    template <typename ItemType>
    template <typename T>
    inline T Span<ItemType>::NumItems() const
    {
        return T(m_numItems);
    }

    template <typename ItemType>
    inline size_t Span<ItemType>::Size() const
    {
        return sizeof(ItemType) * m_numItems;
    }

    template <typename ItemType>
    inline size_t Span<ItemType>::ItemSize()
    {
        return sizeof(ItemType);
    }

    template <typename ItemType>
    inline int64_t Span<ItemType>::Index(const ItemType& item) const
    {
        auto index = &item - m_items;
        assert(index >= 0 && index < m_numItems);
        return index;
    }

    template <typename ItemType>
    inline bool Span<ItemType>::IsEmpty() const
    {
        return m_numItems == 0;
    }

    template <typename ItemType>
    inline bool Span<ItemType>::IsNotEmpty() const
    {
        return m_numItems != 0;
    }

    template <typename ItemType>
    template <typename OtherItemType>
    inline bool Span<ItemType>::IsEqual(const Span<OtherItemType>& otherSpan) const
    {
        auto numItems = m_numItems;
        if (numItems == otherSpan.m_numItems)
        {
            auto items = m_items;
            auto otherItems = otherSpan.m_items;
            while (numItems--)
            {
                if (*items++ != *otherItems++)
                    return false;
            }
            return true;
        }
        return false;
    }

    template <typename ItemType>
    template <typename OtherItemType>
    inline bool Span<ItemType>::IsNotEqual(const Span<OtherItemType>& otherSpan) const
    {
        return !IsEqual(otherSpan);
    }


    template <typename ItemType>
    inline bool Span<ItemType>::operator==(const Span& otherSpan) const
    {
        return IsEqual(otherSpan);
    }

    template <typename ItemType>
    inline bool Span<ItemType>::operator!=(const Span& otherSpan) const
    {
        return IsNotEqual(otherSpan);
    }

    template <typename ItemType>
    inline ItemType& Span<ItemType>::operator[](size_t index)
    {
        assert(index < size_t(m_numItems));
        return m_items[index];
    }

    template <typename ItemType>
    inline const ItemType& Span<ItemType>::operator[](size_t index) const
    {
        assert(index < size_t(m_numItems));
        return m_items[index];
    }

    template <typename ItemType>
    inline ItemType& Span<ItemType>::Last() const
    {
        assert(m_numItems != 0);
        return m_items[m_numItems - 1];
    }

    template <typename ItemType>
    template <typename OtherItemType>
    inline OtherItemType* Span<ItemType>::Items(size_t index) const
    {
        return reinterpret_cast<OtherItemType*>(m_items + index);
    }

    template <typename ItemType>
    template <typename SearchType>
    inline const ItemType* Span<ItemType>::Find(const SearchType& searchItem) const
    {
        for (auto& item : *this)
        {
            if (item == searchItem)
                return &item;
        }
        return nullptr;
    }

    template <typename ItemType>
    inline ItemType* Span<ItemType>::begin()
    {
        return m_items;
    }

    template <typename ItemType>
    inline ItemType* Span<ItemType>::end()
    {
        return m_items + m_numItems;
    }

    template <typename ItemType>
    inline const ItemType* Span<ItemType>::begin() const
    {
        return m_items;
    }

    template <typename ItemType>
    inline const ItemType* Span<ItemType>::end() const
    {
        return m_items + m_numItems;
    }
}
// namespace core