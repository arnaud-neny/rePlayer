#pragma once

#include <Core.h>

namespace core
{
    template <typename ItemType>
    class Array
    {
    public:
        // Setup
        Array() = default;
        Array(size_t numItems, size_t numReservedItems = 0);
        Array(const ItemType* otherItems, size_t numOtherItems);
        Array(const Array& otherArray);
        Array(Array&& otherArray);

        ~Array();

        // States
        template <typename T = uint32_t>
        T NumItems() const;
        template <typename T = size_t>
        T Size() const;
        template <typename T = size_t>
        static T ItemSize();

        bool IsEmpty() const;
        bool IsNotEmpty() const;

        bool operator==(const Array& otherArray) const;

        template <typename ReturnType = ItemType*, typename SearchType>
        ReturnType Find(const SearchType& searchItem) const;
        template <typename ReturnType = ItemType*, typename Predicate>
        ReturnType FindIf(Predicate&& predicate) const;

        // Accessors
        ItemType& operator[](size_t index);
        const ItemType& operator[](size_t index) const;

        ItemType& Last() const;

        template <typename OtherItemType = ItemType>
        OtherItemType* Items(size_t index = 0) const;

        // Modifiers
        Array& operator=(const Array& otherArray);
        Array& operator=(Array&& otherArray);

        template <typename ReturnType = ItemType*>
        ReturnType Push(uint32_t numItems = 1);
        ItemType Pop(size_t numItems = 1);

        template <typename OtherItemType = ItemType, typename ReturnType = ItemType*>
        ReturnType Add(const OtherItemType& otherItem);
        template <typename OtherItemType = ItemType, typename ReturnType = ItemType*>
        ReturnType Add(OtherItemType&& otherItem);
        template <typename OtherItemType, typename ReturnType = ItemType*>
        ReturnType Add(const OtherItemType* otherItems, size_t numOtherItems);

        std::pair<ItemType*, bool> AddOnce(const ItemType& otherItem);

        template <typename OtherItemType = ItemType>
        ItemType* Insert(size_t index, const OtherItemType& otherItem);
        template <typename OtherItemType = ItemType>
        ItemType* Insert(size_t index, OtherItemType&& otherItem);
        template <typename OtherItemType = ItemType>
        ItemType* Insert(size_t index, const OtherItemType* otherItems, size_t numOtherItems);

        void RemoveAtFast(size_t index, size_t numItemsToRemove = 1);
        void RemoveAt(size_t index, size_t numItemsToRemove = 1);
        template <typename SearchType, typename OnFound>
        const int64_t Remove(const SearchType& searchItem, size_t index = 0, OnFound&& onFound = [](const ItemType&) {});
        template <typename SearchType>
        const int64_t Remove(const SearchType& searchItem, size_t index = 0);
        template <typename Predicate>
        uint32_t RemoveIf(Predicate&& predicate); // return the number of items removed

        Array& Swap(Array& otherArray);

        // Storage
        Array& Reset();
        ItemType* Detach();

        Array& Clear();
        Array& Resize(size_t numItems);
        Array& Reserve(size_t numOtherItems);

        // Stl
        ItemType* begin();
        ItemType* end();
        const ItemType* begin() const;
        const ItemType* end() const;

    private:
        ItemType* Grow(size_t oldNumItems, size_t newNumItems);

    private:
        ItemType* m_items = nullptr;
        uint32_t m_numItems = 0;
        uint32_t m_maxItems = 0;
    };
}
// namespace core

#include "Array.inl.h"