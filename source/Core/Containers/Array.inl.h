#pragma once

#include "Array.h"
#include <Core.h>

namespace core
{
    template <typename ItemType>
    inline Array<ItemType>::Array(size_t numItems, size_t numReservedItems)
        : m_numItems(uint32_t(numItems))
        , m_maxItems(uint32_t(numItems + numReservedItems))
        , m_items(Alloc<ItemType>((numItems + numReservedItems) * sizeof(ItemType)))
    {
        if constexpr (!std::is_trivially_constructible<ItemType>::value)
        {
            auto items = m_items;
            while (numItems--)
                new (items++) ItemType();
        }
    }

    template <typename ItemType>
    inline Array<ItemType>::Array(const ItemType* otherItems, size_t numOtherItems)
        : m_numItems(uint32_t(numOtherItems))
        , m_maxItems(uint32_t(numOtherItems))
        , m_items(Alloc<ItemType>(numOtherItems * sizeof(ItemType)))
    {
        // copy
        auto items = m_items;
        if constexpr (std::is_trivially_copyable<ItemType>::value)
            memcpy(items, otherItems, numOtherItems * sizeof(ItemType));
        else
            while (numOtherItems--)
                new (items++) ItemType(*otherItems++);
    }

    template <typename ItemType>
    inline Array<ItemType>::Array(const Array& otherArray)
        : Array(otherArray.m_items, otherArray.m_numItems)
    {}

    template <typename ItemType>
    inline Array<ItemType>::Array(Array&& otherArray)
        : m_items(otherArray.m_items)
        , m_numItems(otherArray.m_numItems)
        , m_maxItems(otherArray.m_maxItems)
    {
        otherArray.m_items = nullptr;
        otherArray.m_numItems = otherArray.m_maxItems = 0;
    }

    template <typename ItemType>
    inline Array<ItemType>::~Array()
    {
        auto items = m_items;
        // delete old items
        if constexpr (!std::is_trivially_destructible<ItemType>::value)
        {
            auto oldItems = items;
            for (auto num = m_numItems; num--; ++oldItems)
                oldItems->~ItemType();
        }
        Free(items);
    }

    template <typename ItemType>
    template <typename T>
    inline T Array<ItemType>::NumItems() const
    {
        return T(m_numItems);
    }

    template <typename ItemType>
    template <typename T>
    inline T Array<ItemType>::Size() const
    {
        return T(sizeof(ItemType) * m_numItems);
    }

    template <typename ItemType>
    template <typename T>
    inline T Array<ItemType>::ItemSize()
    {
        return T(sizeof(ItemType));
    }

    template <typename ItemType>
    inline bool Array<ItemType>::IsEmpty() const
    {
        return m_numItems == 0;
    }

    template <typename ItemType>
    inline bool Array<ItemType>::IsNotEmpty() const
    {
        return m_numItems != 0;
    }

    template <typename ItemType>
    bool Array<ItemType>::operator==(const Array& otherArray) const
    {
        auto numItems = m_numItems;
        if (numItems == otherArray.m_numItems)
        {
            auto items = m_items;
            auto otherItems = otherItems;
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
    template <typename ReturnType, typename SearchType>
    inline ReturnType Array<ItemType>::Find(const SearchType& searchItem) const
    {
        auto items = m_items;
        auto numItems = m_numItems;
        for (ItemType* item = items, *e = item + numItems; item < e; ++item)
        {
            if (*item == searchItem)
            {
                if constexpr (std::is_pointer<ReturnType>::value)
                    return reinterpret_cast<ReturnType>(item);
                else if constexpr (std::is_reference<ReturnType>::value)
                    return reinterpret_cast<ReturnType>(*item);
                else
                    return static_cast<ReturnType>(item - items);
            }
        }
        if constexpr (std::is_pointer<ReturnType>::value)
            return nullptr;
        else if constexpr (std::is_reference<ReturnType>::value)
            return ReturnType();
        else
            return ReturnType(-1);
    }

    template <typename ItemType>
    template <typename ReturnType, typename Predicate>
    inline ReturnType Array<ItemType>::FindIf(Predicate&& predicate) const
    {
        auto items = m_items;
        auto numItems = m_numItems;
        for (ItemType* item = items, *e = item + numItems; item < e; ++item)
        {
            if (std::forward<Predicate>(predicate)(*item))
            {
                if constexpr (std::is_pointer<ReturnType>::value)
                    return reinterpret_cast<ReturnType>(item);
                else if constexpr (std::is_reference<ReturnType>::value)
                    return reinterpret_cast<ReturnType>(*item);
                else
                    return ReturnType(item - items);
            }
        }
        if constexpr (std::is_pointer<ReturnType>::value)
            return nullptr;
        else if constexpr (std::is_reference<ReturnType>::value)
            return ReturnType();
        else
            return ReturnType(-1);
    }

    template <typename ItemType>
    inline ItemType& Array<ItemType>::operator[](size_t index)
    {
        assert(index < size_t(m_numItems));
        return m_items[index];
    }

    template <typename ItemType>
    inline const ItemType& Array<ItemType>::operator[](size_t index) const
    {
        assert(index < size_t(m_numItems));
        return m_items[index];
    }

    template <typename ItemType>
    inline ItemType& Array<ItemType>::Last() const
    {
        assert(m_numItems != 0);
        return m_items[m_numItems - 1];
    }

    template <typename ItemType>
    template <typename OtherItemType>
    inline OtherItemType* Array<ItemType>::Items(size_t index) const
    {
        return reinterpret_cast<OtherItemType*>(m_items + index);
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::operator=(const Array& otherArray)
    {
        assert(!m_items || m_items != otherArray.m_items);
        auto otherNumItems = otherArray.m_numItems;
        if (otherNumItems > m_maxItems)
        {
            // allocate new items
            auto otherItems = otherArray.m_items;
            auto items = Alloc<ItemType>(otherNumItems * sizeof(ItemType));
            // copy
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                memcpy(items, otherItems, otherNumItems * sizeof(ItemType));
            }
            else
            {
                auto itemsIt = items;
                for (auto num = otherNumItems; num--;)
                    new (itemsIt++) ItemType(*otherItems++);
            }
            // delete old items
            if constexpr (!std::is_trivially_destructible<ItemType>::value)
            {
                auto numOldItems = m_numItems;
                for (auto oldItems = m_items; numOldItems--; ++oldItems)
                    oldItems->~ItemType();
            }
            Free(m_items);
            m_items = items;
            m_numItems = m_maxItems = otherNumItems;
        }
        else
        {
            auto items = m_items;
            auto oldNumItems = m_numItems;
            if (otherNumItems > 0)
            {
                // copy
                auto otherItems = otherArray.m_items;
                if constexpr (std::is_trivially_copyable<ItemType>::value)
                {
                    memcpy(items, otherItems, otherNumItems * sizeof(ItemType));
                    items += otherNumItems;
                }
                else
                {
                    auto numToCopy = Min(oldNumItems, otherNumItems);
                    uint32_t itemIndex = 0;
                    for (; itemIndex < numToCopy; itemIndex++)
                            *items++ = *otherItems++;
                    for (; itemIndex < otherNumItems; itemIndex++)
                        new (items++) ItemType(*otherItems++);
                }
            }
            // delete remaining old items
            if constexpr (!std::is_trivially_destructible<ItemType>::value)
            {
                for (; oldNumItems-- > otherNumItems; ++items)
                    items->~ItemType();
            }
            m_numItems = otherNumItems;
        }
        return *this;
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::operator=(Array&& otherArray)
    {
        auto items = m_items;
        auto otherItems = otherArray.m_items;
        assert(!items || items != otherItems);
        // delete old items
        if constexpr (!std::is_trivially_destructible<ItemType>::value)
        {
            auto oldItems = items;
            for (auto num = m_numItems; num--; ++oldItems)
                oldItems->~ItemType();
        }
        Free(items);
        // copy array
        m_items = otherItems;
        m_numItems = otherArray.m_numItems;
        m_maxItems = otherArray.m_maxItems;
        // reset other array
        otherArray.m_items = nullptr;
        otherArray.m_numItems = 0;
        otherArray.m_maxItems = 0;
        return *this;
    }

    template <typename ItemType>
    template <typename ReturnType>
    inline ReturnType Array<ItemType>::Push(uint32_t numItems)
    {
        auto items = m_items;
        auto oldNumItems = m_numItems;
        auto newNumItems = oldNumItems + numItems;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(oldNumItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
                memcpy(newItems, items, oldNumItems * sizeof(ItemType));
            else for (size_t i = 0; i < oldNumItems; ++i)
            {
                new (newItems + i) ItemType(std::move(items[i]));
                items[i].~ItemType();
            }
            Free(items);
            items = newItems;
        }
        items += oldNumItems;
        m_numItems = newNumItems;
        if constexpr (!std::is_trivially_constructible<ItemType>::value)
        {
            for (auto newItems = items; numItems--;)
                new (newItems++) ItemType();
        }
        if constexpr (std::is_pointer<ReturnType>::value)
            return reinterpret_cast<ReturnType>(items);
        else if constexpr (std::is_reference<ReturnType>::value)
            return reinterpret_cast<ReturnType>(*items);
        else
            return static_cast<ReturnType>(oldNumItems);
    }

    template <typename ItemType>
    ItemType Array<ItemType>::Pop(size_t numItems)
    {
        auto oldNumItems = m_numItems;
        assert(numItems > 0 && numItems <= oldNumItems);
        auto newNumItems = oldNumItems - numItems;
        auto items = m_items + newNumItems;
        auto item = std::move(*items);
        if constexpr (!std::is_trivially_destructible<ItemType>::value)
        {
            while (numItems--)
            {
                items->~ItemType();
                ++items;
            }
        }
        m_numItems = uint32_t(newNumItems);
        return item;
    }

    template <typename ItemType>
    template <typename ReturnType>
    inline ReturnType Array<ItemType>::Add(const ItemType& otherItem)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + 1;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
                memcpy(newItems, items, numItems * sizeof(ItemType));
            else for (size_t i = 0; i < numItems; ++i)
            {
                new (newItems + i) ItemType(std::move(items[i]));
                items[i].~ItemType();
            }
            Free(items);
            items = newItems;
        }
        m_numItems = newNumItems;
        auto newItem = new (items + numItems) ItemType(otherItem);
        if constexpr (std::is_pointer<ReturnType>::value)
            return reinterpret_cast<ReturnType>(newItem);
        else if constexpr (std::is_reference<ReturnType>::value)
            return reinterpret_cast<ReturnType>(*newItem);
        else
            return static_cast<ReturnType>(numItems);
    }

    template <typename ItemType>
    template <typename ReturnType>
    inline ReturnType Array<ItemType>::Add(ItemType&& otherItem)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + 1;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
                memcpy(newItems, items, numItems * sizeof(ItemType));
            else for (size_t i = 0; i < numItems; ++i)
            {
                new (newItems + i) ItemType(std::move(items[i]));
                items[i].~ItemType();
            }
            Free(items);
            items = newItems;
        }
        m_numItems = newNumItems;
        auto newItem = new (items + numItems) ItemType(std::move(otherItem));
        if constexpr (std::is_pointer<ReturnType>::value)
            return reinterpret_cast<ReturnType>(newItem);
        else if constexpr (std::is_reference<ReturnType>::value)
            return reinterpret_cast<ReturnType>(*newItem);
        else
            return static_cast<ReturnType>(numItems);
    }

    template <typename ItemType>
    template <typename ReturnType>
    inline ReturnType Array<ItemType>::Add(const ItemType* otherItems, size_t numOtherItems)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + numOtherItems;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                memcpy(newItems, items, numItems * sizeof(ItemType));
                memcpy(newItems + numItems, otherItems, numOtherItems * sizeof(ItemType));
            }
            else
            {
                for (size_t i = 0; i < numItems; ++i)
                {
                    new (newItems + i) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
                for (size_t i = 0; i < numOtherItems; ++i)
                    new (newItems + i + numItems) ItemType(otherItems[i]);
            }
            Free(items);
            items = newItems;
        }
        else
        {
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                memcpy(items + numItems, otherItems, numOtherItems * sizeof(ItemType));
            }
            else
            {
                for (size_t i = 0; i < numOtherItems; ++i)
                    new (items + i + numItems) ItemType(otherItems[i]);
            }
        }
        m_numItems = uint32_t(newNumItems);
        if constexpr (std::is_pointer<ReturnType>::value)
            return reinterpret_cast<ReturnType>(items + numItems);
        else if constexpr (std::is_reference<ReturnType>::value)
            return reinterpret_cast<ReturnType>(items[numItems]);
        else
            return static_cast<ReturnType>(numItems);
    }

    template <typename ItemType>
    template <typename ReturnType>
    inline ReturnType Array<ItemType>::Add(const ItemType& otherItem, size_t numOtherItems)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + uint32_t(numOtherItems);
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
                memcpy(newItems, items, numItems * sizeof(ItemType));
            else for (size_t i = 0; i < numItems; ++i)
            {
                new (newItems + i) ItemType(std::move(items[i]));
                items[i].~ItemType();
            }
            Free(items);
            items = newItems;
        }
        m_numItems = newNumItems;
        items += numItems;
        auto newItem = items;
        while (numOtherItems--)
            new (items++) ItemType(otherItem);
        if constexpr (std::is_pointer<ReturnType>::value)
            return reinterpret_cast<ReturnType>(newItem);
        else if constexpr (std::is_reference<ReturnType>::value)
            return reinterpret_cast<ReturnType>(*newItem);
        else
            return static_cast<ReturnType>(numItems);
    }

    template <typename ItemType>
    inline std::pair<ItemType*, bool> Array<ItemType>::AddOnce(const ItemType& otherItem)
    {
        for (auto& item : *this)
        {
            if (item == otherItem)
                return { &item, false };
        }
        return { Add(otherItem), true };
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::Insert(size_t index, const ItemType& otherItem)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + 1;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                if (index > 0)
                    memcpy(newItems, items, index * sizeof(ItemType));
                newItems[index] = otherItem;
                if (index < numItems)
                    memcpy(newItems + index + 1, items + index, (numItems - index) * sizeof(ItemType));
            }
            else
            {
                for (size_t i = 0; i < index; ++i)
                {
                    new (newItems + i) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
                new (newItems + index) ItemType(otherItem);
                for (size_t i = index; i < numItems; ++i)
                {
                    new (newItems + i + 1) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
            }
            Free(items);
            items = newItems;
        }
        else
        {
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                if (index < numItems)
                    memmove(items + index + 1, items + index, (numItems - index) * sizeof(ItemType));
                items[index] = otherItem;
            }
            else
            {
                for (size_t i = index; i < numItems; ++i)
                    new (items + numItems - i) ItemType(std::move(items[numItems - i - 1]));
                new (items + index) ItemType(otherItem);
            }
        }
        m_numItems = uint32_t(newNumItems);
        return items + index;
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::Insert(size_t index, ItemType&& otherItem)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + 1;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                if (index > 0)
                    memcpy(newItems, items, index * sizeof(ItemType));
                newItems[index] = std::move(otherItem);
                if (index < numItems)
                    memcpy(newItems + index + 1, items + index, (numItems - index) * sizeof(ItemType));
            }
            else
            {
                for (size_t i = 0; i < index; ++i)
                {
                    new (newItems + i) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
                new (newItems + index) ItemType(std::move(otherItem));
                for (size_t i = index; i < numItems; ++i)
                {
                    new (newItems + i + 1) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
            }
            Free(items);
            items = newItems;
        }
        else
        {
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                if (index < numItems)
                    memmove(items + index + 1, items + index, (numItems - index) * sizeof(ItemType));
                items[index] = otherItem;
            }
            else
            {
                for (size_t i = index; i < numItems; ++i)
                    new (items + numItems - i) ItemType(std::move(items[numItems - i - 1]));
                new (items + index) ItemType(std::move(otherItem));
            }
        }
        m_numItems = uint32_t(newNumItems);
        return items + index;
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::Insert(size_t index, const ItemType* otherItems, size_t numOtherItems)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + numOtherItems;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                if (index > 0)
                    memcpy(newItems, items, index * sizeof(ItemType));
                memcpy(newItems + index, otherItems, numOtherItems * sizeof(ItemType));
                if (index < numItems)
                    memcpy(newItems + index + numOtherItems, items + index, (numItems - index) * sizeof(ItemType));
            }
            else
            {
                for (size_t i = 0; i < index; ++i)
                {
                    new (newItems + i) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
                for (size_t i = 0; i < numOtherItems; ++i)
                    new (newItems + i + index) ItemType(otherItems[i]);
                for (size_t i = index; i < numItems; ++i)
                {
                    new (newItems + i + numOtherItems) ItemType(std::move(items[i]));
                    items[i].~ItemType();
                }
            }
            Free(items);
            items = newItems;
        }
        else
        {
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                if (index < numItems)
                    memmove(items + index + numOtherItems, items + index, (numItems - index) * sizeof(ItemType));
                memcpy(items + index, otherItems, numOtherItems * sizeof(ItemType));
            }
            else
            {
                for (size_t i = index; i < numItems; ++i)
                    new (items + numItems - i - 1 + numOtherItems) ItemType(std::move(items[numItems - i - 1]));
                for (size_t i = 0; i < numOtherItems; ++i)
                    new (items + i + index) ItemType(otherItems[i]);
            }
        }
        m_numItems = uint32_t(newNumItems);
        return items + index;
    }

    template <typename ItemType>
    template <typename OtherItemType, typename ReturnType>
    inline ReturnType Array<ItemType>::Copy(const OtherItemType* otherItems, size_t size)
    {
        static_assert(std::is_trivially_copyable<ItemType>::value && std::is_trivially_copyable<OtherItemType>::value);
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + size / sizeof(ItemType);
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            memcpy(newItems, items, numItems * sizeof(ItemType));
            memcpy(newItems + numItems, otherItems, size);
            Free(items);
            items = newItems;
        }
        else
        {
            memcpy(items + numItems, otherItems, size);
        }
        m_numItems = uint32_t(newNumItems);
        if constexpr (std::is_pointer<ReturnType>::value)
            return reinterpret_cast<ReturnType>(items + numItems);
        else if constexpr (std::is_reference<ReturnType>::value)
            return reinterpret_cast<ReturnType>(items[numItems]);
        else
            return static_cast<ReturnType>(numItems);
    }

    template <typename ItemType>
    void Array<ItemType>::RemoveAtFast(size_t index, size_t numItemsToRemove)
    {
        auto numItems = m_numItems;
        auto newNumItems = numItems - numItemsToRemove;
        assert(newNumItems <= numItems && (index + numItemsToRemove <= numItems));
        auto* items = m_items;
        auto* dstItems = items + index;
        auto* movedItems = items + numItems - numItemsToRemove;
        if constexpr (std::is_trivially_copyable<ItemType>::value)
        {
            memcpy(dstItems, movedItems, numItemsToRemove * sizeof(ItemType));
            movedItems += numItemsToRemove;
        }
        else
        {
            for (auto num = numItemsToRemove; num--;)
                *dstItems++ = std::move(*movedItems++);
        }
        if constexpr (!std::is_trivially_destructible<ItemType>::value)
        {
            while (numItemsToRemove--)
                (--movedItems)->~ItemType();
        }
        m_numItems = uint32_t(newNumItems);
    }

    template <typename ItemType>
    void Array<ItemType>::RemoveAt(size_t index, size_t numItemsToRemove)
    {
        auto numItems = m_numItems;
        auto newNumItems = numItems - numItemsToRemove;
        assert(newNumItems <= numItems && (index + numItemsToRemove <= numItems));
        auto items = m_items + index;
        auto movedItems = items + numItemsToRemove;
        auto numItemsToMove = newNumItems - index;
        if constexpr (std::is_trivially_copyable<ItemType>::value)
        {
            memmove(items, movedItems, numItemsToMove * sizeof(ItemType));
            items += numItemsToMove;
        }
        else
        {
            while (numItemsToMove--)
                *items++ = std::move(*movedItems++);
        }
        if constexpr (!std::is_trivially_destructible<ItemType>::value)
        {
            while (numItemsToRemove--)
                (items++)->~ItemType();
        }
        m_numItems = uint32_t(newNumItems);
    }

    template <typename ItemType>
    template <typename SearchType, typename OnFound>
    const int64_t Array<ItemType>::Remove(const SearchType& searchItem, size_t index, OnFound&& onFound)
    {
        auto numItems = m_numItems;
        auto items = m_items + index;
        for (; index < numItems; index++, items++)
        {
            if (*items == searchItem)
            {
                std::forward<OnFound>(onFound)(*items);
                RemoveAt(index);
                return index;
            }
        }
        return -1;
    }

    template <typename ItemType>
    template <typename SearchType>
    const int64_t Array<ItemType>::Remove(const SearchType& searchItem, size_t index)
    {
        return Remove(searchItem, index, [](const ItemType&) {});
    }

    template <typename ItemType>
    template <typename Predicate>
    uint32_t Array<ItemType>::RemoveIf(Predicate&& predicate)
    {
        auto numItems = m_numItems;
        auto items = m_items;
        for (auto e = items + numItems; items < e; items++)
        {
            if (std::forward<Predicate>(predicate)(*items))
            {
                auto movedItems = items++;
                for (; items < e; items++)
                {
                    if (!std::forward<Predicate>(predicate)(*items))
                        *movedItems++ = std::move(*items);
                }
                auto numRemovedItems = uint32_t(items - movedItems);
                if constexpr (!std::is_trivially_destructible<ItemType>::value)
                {
                    while (movedItems != items)
                        (movedItems++)->~ItemType();
                }
                m_numItems = numItems - numRemovedItems;
                return numRemovedItems;
            }
        }
        return 0;
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::Swap(Array& otherArray)
    {
        std::swap(m_items, otherArray.m_items);
        std::swap(m_numItems, otherArray.m_numItems);
        std::swap(m_maxItems, otherArray.m_maxItems);
        return *this;
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::Reset()
    {
        auto items = m_items;
        // delete old items
        if constexpr (!std::is_trivially_destructible<ItemType>::value)
        {
            auto oldItems = items;
            for (auto num = m_numItems; num--; ++oldItems)
                oldItems->~ItemType();
        }
        Free(items);
        m_items = nullptr;
        m_numItems = m_maxItems = 0;
        return *this;
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::Detach()
    {
        auto items = m_items;
        m_items = nullptr;
        m_numItems = m_maxItems = 0;
        return items;
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::Clear()
    {
        Resize(0);
        return *this;
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::Resize(size_t numItems)
    {
        auto oldItems = m_items;
        auto oldNumItems = size_t(m_numItems);
        auto oldMaxItems = size_t(m_maxItems);
        m_numItems = uint32_t(numItems);
        if (numItems > oldMaxItems)
        {
            auto newItems = Alloc<ItemType>(numItems * sizeof(ItemType));
            auto items = newItems;
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                memcpy(items, oldItems, oldNumItems * sizeof(ItemType));
                items += oldNumItems;
            }
            else
            {
                auto otherItems = oldItems;
                for (auto num = oldNumItems; num--;)
                {
                    new (items++) ItemType(std::move(*otherItems));
                    otherItems->~ItemType();
                    otherItems++;
                }
            }
            if constexpr (!std::is_trivially_constructible<ItemType>::value)
            {
                for (; oldNumItems < numItems; oldNumItems++)
                    new (items++) ItemType();
            }
            Free(oldItems);
            m_items = newItems;
            m_maxItems = uint32_t(numItems);
        }
        else if (numItems > oldNumItems)
        {
            // construct new items
            if constexpr (!std::is_trivially_constructible<ItemType>::value)
            {
                for (; oldNumItems < numItems; ++oldNumItems)
                    new (oldItems + oldNumItems) ItemType();
            }
        }
        else
        {
            // delete old items
            if constexpr (!std::is_trivially_destructible<ItemType>::value)
            {
                for (; numItems < oldNumItems; ++numItems)
                    oldItems[numItems].~ItemType();
            }
        }
        return *this;
    }

    template <typename ItemType>
    inline Array<ItemType>& Array<ItemType>::Reserve(size_t numOtherItems)
    {
        auto items = m_items;
        auto numItems = m_numItems;
        auto newNumItems = numItems + numOtherItems;
        if (newNumItems > m_maxItems)
        {
            auto newItems = Grow(numItems, newNumItems);
            if constexpr (std::is_trivially_copyable<ItemType>::value)
            {
                memcpy(newItems, items, numItems * sizeof(ItemType));
            }
            else
            {
                auto oldItems = items;
                while (numItems--)
                {
                    new (newItems++) ItemType(std::move(*oldItems));
                    oldItems->~ItemType();
                    oldItems++;
                }
            }
            Free(items);
        }
        return *this;
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::begin()
    {
        return m_items;
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::end()
    {
        return m_items + m_numItems;
    }

    template <typename ItemType>
    inline const ItemType* Array<ItemType>::begin() const
    {
        return m_items;
    }

    template <typename ItemType>
    inline const ItemType* Array<ItemType>::end() const
    {
        return m_items + m_numItems;
    }

    template <typename ItemType>
    inline ItemType* Array<ItemType>::Grow(size_t oldNumItems, size_t newNumItems)
    {
        // 50% grow from the current number of items
        auto growSize = (oldNumItems * 3 + 1) / 2;
        // pick the larger
        auto maxItems = Max(newNumItems, growSize);
        // forbid useless calls to Grow
        assert(maxItems > m_maxItems);

        auto* items = Alloc<ItemType>(maxItems * sizeof(ItemType));
        m_items = items;
        m_maxItems = uint32_t(maxItems);
        return items;
    }

    template <typename ItemType>
    inline uint32_t operator-(const ItemType* const item, const Array<ItemType>& array)
    {
        auto* items = array.Items();
        assert((item == nullptr && items == nullptr) || (item >= items) && (item <= items + array.NumItems()));
        return uint32_t(item - items); // returns the index of element
    }
}
// namespace core