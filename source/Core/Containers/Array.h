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
        Array(uint32_t numItems, uint32_t numReservedItems = 0);
        Array(const ItemType* otherItems, uint32_t numOtherItems);
        Array(const Array& otherArray);
        Array(Array&& otherArray);

        ~Array();

        // States
        template <typename T = uint32_t>
        T NumItems() const;
        template <typename T = uint64_t>
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
        template <typename T = uint32_t>
        ItemType& operator[](T index);
        template <typename T = uint32_t>
        const ItemType& operator[](T index) const;

        ItemType& Last() const;

        template <typename OtherItemType = ItemType, typename T = uint32_t>
        OtherItemType* Items(T index = 0) const;

        // Modifiers
        Array& operator=(const Array& otherArray);
        Array& operator=(Array&& otherArray);

        template <typename ReturnType = ItemType*>
        ReturnType Push(uint32_t numItems = 1);
        ItemType Pop(uint32_t numItems = 1);

        template <typename ReturnType = ItemType*>
        ReturnType Add(const ItemType& otherItem);
        template <typename ReturnType = ItemType*>
        ReturnType Add(ItemType&& otherItem);
        template <typename ReturnType = ItemType*>
        ReturnType Add(const ItemType* otherItems, uint32_t numOtherItems);
        template <typename ReturnType = ItemType*>
        ReturnType Add(const ItemType& otherItem, uint32_t numOtherItems);

        std::pair<ItemType*, bool> AddOnce(const ItemType& otherItem);

        template <typename T = uint32_t>
        ItemType* Insert(T index, const ItemType& otherItem);
        template <typename T = uint32_t>
        ItemType* Insert(T index, ItemType&& otherItem);
        template <typename T = uint32_t>
        ItemType* Insert(T index, const ItemType* otherItems, uint32_t numOtherItems);

        template <typename OtherItemType, typename ReturnType = ItemType*>
        ReturnType Copy(const OtherItemType* otherItems, uint64_t size);

        template <typename T = uint32_t>
        void RemoveAtFast(T index, uint32_t numItemsToRemove = 1);
        template <typename T = uint32_t>
        void RemoveAt(T index, uint32_t numItemsToRemove = 1);
        template <typename SearchType, typename OnFound, typename T = uint32_t>
        const int64_t Remove(const SearchType& searchItem, T index = 0u, OnFound&& onFound = [](const ItemType&) {});
        template <typename SearchType, typename T = uint32_t>
        const int64_t Remove(const SearchType& searchItem, T index = 0u);
        template <typename Predicate>
        uint32_t RemoveIf(Predicate&& predicate); // return the number of items removed

        Array& Swap(Array& otherArray);

        // Storage
        Array& Reset();
        ItemType* Detach();

        Array& Clear();
        Array& Resize(uint32_t numItems);
        Array& Reserve(uint32_t numOtherItems);

        // Stl
        ItemType* begin();
        ItemType* end();
        const ItemType* begin() const;
        const ItemType* end() const;

    private:
        ItemType* Grow(uint32_t oldNumItems, uint32_t newNumItems);

    private:
        ItemType* m_items = nullptr;
        uint32_t m_numItems = 0;
        uint32_t m_maxItems = 0;
    };
}
// namespace core

#include "Array.inl.h"