#pragma once

#include <Core/Types.h>

namespace core
{
    template <typename ItemType>
    class Span
    {
        template <typename OtherItemType>
        friend class Span;
    public:
        // Setup
        Span(ItemType* items, size_t numItems);
        Span(ItemType* beginItems, ItemType* endItems);
        Span(const Span& otherSpan);
        Span(Span&& otherSpan);

        // States
        template <typename T = uint32_t>
        T NumItems() const;
        size_t Size() const;
        static size_t ItemSize();

        int64_t Index(const ItemType& item) const;

        bool IsEmpty() const;
        bool IsNotEmpty() const;

        template <typename OtherItemType>
        bool IsEqual(const Span<OtherItemType>& otherSpan) const;
        template <typename OtherItemType>
        bool IsNotEqual(const Span<OtherItemType>& otherSpan) const;

        bool operator==(const Span& otherSpan) const;
        bool operator!=(const Span& otherSpan) const;

        // Accessors
        ItemType& operator[](size_t index);
        const ItemType& operator[](size_t index) const;

        ItemType& Last() const;

        template <typename OtherItemType = ItemType>
        OtherItemType* Items(size_t index = 0) const;

        // Modifiers
        Span& operator=(const Span& otherSpan);
        Span& operator=(Span&& otherSpan);

        //
        template <typename SearchType>
        const ItemType* Find(const SearchType& searchItem) const;

        // Stl
        ItemType* begin();
        ItemType* end();
        const ItemType* begin() const;
        const ItemType* end() const;

    private:
        ItemType* m_items = nullptr;
        uint32_t m_numItems = 0;
    };
}
// namespace core

#include "Span.inl.h"