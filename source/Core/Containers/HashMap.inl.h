#pragma once

#include "HashMap.h"
#include "HashTypes.h"

namespace core
{
    template <class KeyType, class ItemType>
    template <typename T>
    inline T* HashMap<KeyType, ItemType>::Entry::Storage<T>::operator()()
    {
        return reinterpret_cast<T*>(m_buffer);
    }

    template <class KeyType, class ItemType>
    template <typename T>
    inline const T* HashMap<KeyType, ItemType>::Entry::Storage<T>::operator()() const
    {
        return reinterpret_cast<T*>(m_buffer);
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Entry::Set(KeyParamType key, const ItemType& item)
    {
        new (m_key.m_buffer) KeyStorageType(key);
        new (m_item.m_buffer) ItemType(item);
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Entry::MoveSet(KeyParamType key, ItemType&& item)
    {
        new (m_key.m_buffer) KeyStorageType(key);
        new (m_item.m_buffer) ItemType(std::move(item));
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Entry::Set(KeyParamType key)
    {
        new (m_key.m_buffer) KeyStorageType(key);
        new (m_item.m_buffer) ItemType();
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Entry::MoveSet(KeyType&& key)
    {
        new (m_key.m_buffer) KeyStorageType(std::move(key));
        new (m_item.m_buffer) ItemType();
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Entry::Clear()
    {
        m_key()->~KeyStorageType();
        m_item()->~ItemType();
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Entry::RelocateTo(Entry& entry)
    {
        memcpy(&entry, this, sizeof(Entry));
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::HashMap(uint32_t maxEntries)
    {
        if (maxEntries)
        {
            m_maxEntries = maxEntries;
            const uint32_t bucketArraySize = (maxEntries * 2 - 1) * sizeof(uint32_t);

            auto buffer = Alloc<uint8_t>(maxEntries * (sizeof(Entry) + sizeof(uint32_t)) + bucketArraySize, alignof(Entry));
            m_entries = reinterpret_cast<Entry*>(buffer);
            uint32_t* bucketArray = reinterpret_cast<uint32_t*>(buffer + maxEntries * (sizeof(Entry) + sizeof(uint32_t)));

            memset(bucketArray, 0xFF, bucketArraySize);
        }
        else
        {
            m_maxEntries = 0;
            m_entries = nullptr;
        }

        m_numEntries = 0;
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::Iterator::Iterator()
        : m_map(nullptr)
        , m_index(0)
    {}

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::Iterator::Iterator(const HashMap& map)
        : m_map(&map)
        , m_index(0)
    {}

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::Iterator::Iterator(const HashMap& map, uint32_t index)
        : m_map(&map)
        , m_index(index)
    {}

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::Iterator::operator==(const Iterator& otherIt) const
    {
        return m_map == otherIt.m_map && m_index == otherIt.m_index;
    }

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::Iterator::operator!=(const Iterator& otherIt) const
    {
        return m_map != otherIt.m_map || m_index != otherIt.m_index;
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::Iterator::operator bool() const
    {
        return m_map && (m_index < m_map->m_numEntries);
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::KeyReturnType HashMap<KeyType, ItemType>::Iterator::Key() const
    {
        return *m_map->m_entries[m_index].m_key();
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::KeyStorageType* HashMap<KeyType, ItemType>::Iterator::KeyPtr() const
    {
        return m_map->m_entries[m_index].m_key();
    }

    template <class KeyType, class ItemType>
    inline ItemType& HashMap<KeyType, ItemType>::Iterator::Item()
    {
        return *m_map->m_entries[m_index].m_item();
    }

    template <class KeyType, class ItemType>
    inline const ItemType& HashMap<KeyType, ItemType>::Iterator::Item() const
    {
        return *m_map->m_entries[m_index].m_item();
    }

    template <class KeyType, class ItemType>
    inline ItemType* HashMap<KeyType, ItemType>::Iterator::operator->()
    {
        return m_map->m_entries[m_index].m_item();
    }

    template <class KeyType, class ItemType>
    inline const ItemType* HashMap<KeyType, ItemType>::Iterator::operator->() const
    {
        return m_map->m_entries[m_index].m_item();
    }

    template <class KeyType, class ItemType>
    inline ItemType& HashMap<KeyType, ItemType>::Iterator::operator*()
    {
        return *m_map->m_entries[m_index].m_item();
    }

    template <class KeyType, class ItemType>
    inline const ItemType& HashMap<KeyType, ItemType>::Iterator::operator*() const
    {
        return *m_map->m_entries[m_index].m_item();
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Iterator::operator++()
    {
        ++m_index;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Iterator::operator++(int)
    {
        ++m_index;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Iterator::operator--()
    {
        --m_index;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Iterator::operator--(int)
    {
        --m_index;
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::HashMap(const HashMap& otherMap)
    {
        m_maxEntries = 0;
        m_entries = nullptr;
        m_numEntries = 0;

        *this = otherMap;
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::HashMap(HashMap&& otherMap)
    {
        m_maxEntries = otherMap.m_maxEntries;
        m_entries = otherMap.m_entries;
        m_numEntries = otherMap.m_numEntries;

        otherMap.m_maxEntries = 0;
        otherMap.m_entries = 0;
        otherMap.m_numEntries = 0;
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::~HashMap()
    {
        auto entries = m_entries;
        for (uint32_t i = 0, numEntries = m_numEntries; i < numEntries; ++i)
        {
            entries[i].Clear();
        }
        Free(entries);
    }

    template <class KeyType, class ItemType>
    inline uint32_t HashMap<KeyType, ItemType>::NumItems() const
    {
        return m_numEntries;
    }

    template <class KeyType, class ItemType>
    inline ItemType& HashMap<KeyType, ItemType>::operator[](KeyParamType key)
    {
        uint32_t keyHash;
        uint32_t bucketIndex;
        {
            keyHash = Hash::Get(key);
            if (m_maxEntries)
            {
                bucketIndex = GetBucketIndex(keyHash);
                uint32_t* next = NextArray();
                auto entries = m_entries;
                for (uint32_t entryIndex = BucketArray()[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries; entryIndex = next[entryIndex])
                {
                    if (Hash::Compare(key, *entries[entryIndex].m_key()))
                    {
                        return *entries[entryIndex].m_item();
                    }
                }
            }
            else
            {
                bucketIndex = 0;
            }
        }

        // Grow if necessary
        const uint32_t newSize = CalcNewArraySize();
        if (newSize > m_maxEntries)
        {
            ResizeBuckets(newSize);
            bucketIndex = GetBucketIndex(keyHash);
        }

        const uint32_t entryIndex = m_numEntries++;
        m_entries[entryIndex].Set(key);
        uint32_t* bucketArray = BucketArray();
        uint32_t* next = NextArray();
        next[entryIndex] = bucketArray[bucketIndex];
        bucketArray[bucketIndex] = entryIndex;

        return *m_entries[entryIndex].m_item();
    }

    template <class KeyType, class ItemType>
    inline ItemType& HashMap<KeyType, ItemType>::operator[](KeyStorageType&& key)
    {
        uint32_t keyHash;
        uint32_t bucketIndex;
        {
            keyHash = Hash::Get(key);
            if (m_maxEntries)
            {
                bucketIndex = GetBucketIndex(keyHash);
                uint32_t* pNext = NextArray();
                auto entries = m_entries;
                for (uint32_t entryIndex = BucketArray()[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries; entryIndex = pNext[entryIndex])
                {
                    if (Hash::Compare(key, *entries[entryIndex].m_key()))
                    {
                        return *entries[entryIndex].m_item();
                    }
                }
            }
            else
            {
                bucketIndex = 0;
            }
        }

        // Grow if necessary
        const uint32_t newSize = CalcNewArraySize();
        if (newSize > m_maxEntries)
        {
            ResizeBuckets(newSize);
            bucketIndex = GetBucketIndex(keyHash);
        }

        const uint32_t entryIndex = m_numEntries++;
        m_entries[entryIndex].MoveSet(std::move(key));
        uint32_t* bucketArray = BucketArray();
        uint32_t* next = NextArray();
        next[entryIndex] = bucketArray[bucketIndex];
        bucketArray[bucketIndex] = entryIndex;

        return *m_entries[entryIndex].m_item();
    }

    template <class KeyType, class ItemType>
    inline const ItemType& HashMap<KeyType, ItemType>::operator[](KeyParamType key) const
    {
        if (m_maxEntries)
        {
            uint32_t bucketIndex = GetBucketIndex(Hash::Get(key));
            uint32_t* next = NextArray();
            auto entries = m_entries;
            for (uint32_t entryIndex = BucketArray()[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries; entryIndex = next[entryIndex])
            {
                if (Hash::Compare(key, *entries[entryIndex].m_key()))
                {
                    return *entries[entryIndex].m_item();
                }
            }
        }

        assert(0 && "Out of bound access");
        static ItemType s_item;
        return s_item;
    }

//     template <class KeyType, class ItemType>
//     inline typename HashMap<KeyType, ItemType>::KeyReturnType HashMap<KeyType, ItemType>::GetKeyByIndex(uint32_t index) const
//     {
//         assert(index < m_numEntries);
//         return *m_entries[index].m_key();
//     }

//     template <class KeyType, class ItemType>
//     inline ItemType& HashMap<KeyType, ItemType>::GetItemByIndex(uint32_t index) const
//     {
//         assert(index < m_numEntries);
//         return *m_entries[index].m_item();
//     }

    template <class KeyType, class ItemType>
    inline const ItemType& HashMap<KeyType, ItemType>::Get(KeyParamType key, const ItemType& defaultItem) const
    {
        uint32_t entryIndex = FindIndexByKey(key);
        if (entryIndex != kInvalidIndex)
            return *m_entries[entryIndex].m_item();
        return defaultItem;
    }

    template <class KeyType, class ItemType>
    inline ItemType* const HashMap<KeyType, ItemType>::FindItemByKey(KeyParamType key) const
    {
        uint32_t entryIndex = FindIndexByKey(key);
        if (entryIndex != kInvalidIndex)
            return const_cast<ItemType* const>(m_entries[entryIndex].m_item());
        return nullptr;
    }

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::FindKeyByItem(const ItemType& item, KeyType& key) const
    {
        auto entries = m_entries;
        for (uint32_t i = 0, numEntries = m_numEntries; i < numEntries; ++i)
        {
            if (Hash::Compare(*entries[i].m_item(), item))
            {
                key = *entries[i].m_key();
                return true;
            }
        }
        return false;
    }

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::PopByKey(KeyParamType key, ItemType& item)
    {
        if (!m_maxEntries)
            return false;

        uint32_t bucketIndex = GetBucketIndex(Hash::Get(key));
        uint32_t prevEntryIndex = kInvalidIndex;
        uint32_t* next = NextArray();
        uint32_t* bucketArray = BucketArray();
        auto entries = m_entries;
        for (uint32_t entryIndex = bucketArray[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries;)
        {
            if (Hash::Compare(key, *entries[entryIndex].m_key()))
            {
                item = *entries[entryIndex].m_item();
                // Remove it from the bucket list
                if (prevEntryIndex < m_numEntries)
                {
                    next[prevEntryIndex] = next[entryIndex];
                }
                else
                {
                    bucketArray[bucketIndex] = next[entryIndex];
                }
                // Remove it from m_entries
                RemoveAtIndex(entryIndex);
                return true;
            }
            prevEntryIndex = entryIndex;
            entryIndex = next[entryIndex];
        }
        return false;
    }

    template <class KeyType, class ItemType>
    inline ItemType& HashMap<KeyType, ItemType>::Insert(KeyParamType key, const ItemType& item)
    {
        uint32_t keyHash;
        uint32_t bucketIndex;
        {
            keyHash = Hash::Get(key);
            if (m_maxEntries)
            {
                bucketIndex = GetBucketIndex(keyHash);
                uint32_t* next = NextArray();
                auto entries = m_entries;
                for (uint32_t entryIndex = BucketArray()[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries; entryIndex = next[entryIndex])
                {
                    if (Hash::Compare(key, *entries[entryIndex].m_key()))
                    {
                        *entries[entryIndex].m_item() = item;
                        return *entries[entryIndex].m_item();
                    }
                }
            }
            else
            {
                bucketIndex = 0;
            }
        }

        // Grow if necessary
        const uint32_t newSize = CalcNewArraySize();
        if (newSize > m_maxEntries)
        {
            ResizeBuckets(newSize);
            bucketIndex = GetBucketIndex(keyHash);
        }

        const uint32_t entryIndex = m_numEntries++;
        m_entries[entryIndex].Set(key, item);
        uint32_t* bucketArray = BucketArray();
        uint32_t* next = NextArray();
        next[entryIndex] = bucketArray[bucketIndex];
        bucketArray[bucketIndex] = entryIndex;

        return *m_entries[entryIndex].m_item();
    }

    template <class KeyType, class ItemType>
    inline ItemType& HashMap<KeyType, ItemType>::Insert(KeyParamType key, ItemType&& item)
    {
        uint32_t keyHash;
        uint32_t bucketIndex;
        {
            keyHash = Hash::Get(key);
            if (m_maxEntries)
            {
                bucketIndex = GetBucketIndex(keyHash);
                uint32_t* next = NextArray();
                auto entries = m_entries;
                for (uint32_t entryIndex = BucketArray()[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries; entryIndex = next[entryIndex])
                {
                    if (Hash::Compare(key, *entries[entryIndex].m_key()))
                    {
                        *entries[entryIndex].m_item() = std::move(item);
                        return *entries[entryIndex].m_item();
                    }
                }
            }
            else
            {
                bucketIndex = 0;
            }
        }

        // Grow if necessary
        const uint32_t newSize = CalcNewArraySize();
        if (newSize > m_maxEntries)
        {
            ResizeBuckets(newSize);
            bucketIndex = GetBucketIndex(keyHash);
        }

        const uint32_t entryIndex = m_numEntries++;
        m_entries[entryIndex].MoveSet(key, std::move(item));
        uint32_t* bucketArray = BucketArray();
        uint32_t* next = NextArray();
        next[entryIndex] = bucketArray[bucketIndex];
        bucketArray[bucketIndex] = entryIndex;

        return *m_entries[entryIndex].m_item();
    }

    template <class KeyType, class ItemType>
    template <typename RunItem>
    inline bool HashMap<KeyType, ItemType>::RemoveByKey(KeyParamType key, RunItem runItem)
    {
        if (!m_maxEntries)
            return false;

        uint32_t bucketIndex = GetBucketIndex(Hash::Get(key));
        uint32_t prevEntryIndex = kInvalidIndex;
        uint32_t* next = NextArray();
        uint32_t* bucketArray = BucketArray();
        auto entries = m_entries;
        for (uint32_t entryIndex = bucketArray[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries;)
        {
            if (Hash::Compare(key, *entries[entryIndex].m_key()))
            {
                runItem(*entries[entryIndex].m_item());

                // Remove it from the bucket list
                if (prevEntryIndex < numEntries)
                {
                    next[prevEntryIndex] = next[entryIndex];
                }
                else
                {
                    bucketArray[bucketIndex] = next[entryIndex];
                }
                // Remove it from m_entries
                RemoveAtIndex(entryIndex);
                return true;
            }
            prevEntryIndex = entryIndex;
            entryIndex = next[entryIndex];
        }
        return false;
    }

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::RemoveByKey(KeyParamType key)
    {
        return RemoveByKey(key, [](const auto&) {});
    }

/*
    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::RemoveByKey(KeyParamType key)
    {
        if (!m_maxEntries)
            return false;

        uint32_t bucketIndex = GetBucketIndex(Hash::Get(key));
        uint32_t prevEntryIndex = kInvalidIndex;
        uint32_t* next = NextArray();
        uint32_t* bucketArray = BucketArray();
        auto entries = m_entries;
        for (uint32_t entryIndex = pBucketArray[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries;)
        {
            if (Hash::Compare(key, *entries[entryIndex].m_key()))
            {
                // Remove it from the bucket list
                if (prevEntryIndex < numEntries)
                {
                    next[prevEntryIndex] = next[entryIndex];
                }
                else
                {
                    bucketArray[bucketIndex] = next[entryIndex];
                }
                // Remove it from m_entries
                RemoveAtIndex(entryIndex);
                return true;
            }
            prevEntryIndex = entryIndex;
            entryIndex = next[entryIndex];
        }
        return false;
    }
*/

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::RemoveByItem(const ItemType& item)
    {
        auto entries = m_entries;
        for (uint32_t i = 0, numEntries = m_numEntries; i < numEntries; ++i)
        {
            if (Hash::Compare(*entries[i].m_item(), item))
            {
                RemoveByKey(*entries[i].m_key());
                return true;
            }
        }
        return false;
    }

    template <class KeyType, class ItemType>
    inline bool HashMap<KeyType, ItemType>::RemoveByHashAndItem(uint32_t hash, const ItemType& item)
    {
        if (!m_maxEntries)
        {
            return false;
        }

        uint32_t bucketIndex = GetBucketIndex(hash);
        uint32_t prevEntryIndex = kInvalidIndex;
        uint32_t* next = NextArray();
        uint32_t* bucketArray = BucketArray();
        auto entries = m_entries;
        for (uint32_t entryIndex = bucketArray[bucketIndex], numEntries = m_numEntries; entryIndex < numEntries;)
        {
            if (Hash::Compare(*entries[entryIndex].m_item(), item))
            {
                assert(hash == Hash::Get(*m_entries[entryIndex].m_key()) && "Hash/key mismatch"/* (hash is % u, key is % s)", hash, HashMapKeyAsString<KeyType>(*m_entries[entryIndex].m_key())*/);
                // Remove it from the bucket list
                if (prevEntryIndex < numEntries)
                {
                    next[prevEntryIndex] = next[entryIndex];
                }
                else
                {
                    bucketArray[bucketIndex] = next[entryIndex];
                }
                // Remove it from m_entries
                RemoveAtIndex(entryIndex);
                return true;
            }
            prevEntryIndex = entryIndex;
            entryIndex = next[entryIndex];
        }
        return false;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::RemoveAndStepIteratorForward(Iterator& iterator)
    {
        assert(iterator.m_index < m_numEntries);

        RemoveByKey(*m_entries[iterator.m_index].m_key());
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::RemoveAll()
    {
        if (m_maxEntries && m_numEntries)
            memset(BucketArray(), 0xFF, BucketArraySize() * sizeof(uint32_t));

        auto entries = m_entries;
        for (uint32_t i = 0, numEntries = m_numEntries; i < numEntries; ++i)
        {
            entries[i].Clear();
        }
        m_numEntries = 0;
    }

    template <class KeyType, class ItemType>
    template <typename Releaser>
    inline void HashMap<KeyType, ItemType>::ReleaseAll(Releaser releaser)
    {
        if (m_maxEntries && m_numEntries)
            memset(BucketArray(), 0xFF, BucketArraySize() * sizeof(uint32_t));

        auto entries = m_entries;
        for (uint32_t i = 0, numEntries = m_numEntries; i < numEntries; ++i)
        {
            releaser(*entries[i].m_item());
            entries[i].Clear();
        }
        m_numEntries = 0;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::DeleteAll()
    {
        ReleaseAll([](auto& item)
        {
            delete item;
        });
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>& HashMap<KeyType, ItemType>::operator=(const HashMap<KeyType, ItemType>& otherMap)
    {
        if (this != &otherMap)
        {
            auto entries = m_entries;
            for (uint32_t i = 0, numEntries = m_numEntries; i < numEntries; ++i)
            {
                entries[i].Clear();
            }

            if (m_maxEntries != otherMap.m_maxEntries)
            {
                m_maxEntries = otherMap.m_maxEntries;

                Free(entries);

                if (m_maxEntries)
                {
                    m_entries = Alloc<Entry>(m_maxEntries * (sizeof(Entry) + sizeof(uint32_t)) + BucketArraySize() * sizeof(uint32_t));
                }
                else
                {
                    m_entries = nullptr;
                    m_numEntries = 0;
                }
            }

            if (m_maxEntries)
            {
                m_numEntries = otherMap.m_numEntries;

                memcpy(NextArray(), otherMap.NextArray(), m_maxEntries * sizeof(uint32_t));
                memcpy(BucketArray(), otherMap.BucketArray(), BucketArraySize() * sizeof(uint32_t));
                for (uint32_t i = 0; i < m_numEntries; ++i)
                {
                    m_entries[i].Set(*otherMap.m_entries[i].m_key(), *otherMap.m_entries[i].m_item());
                }
            }
        }
        return *this;
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>& HashMap<KeyType, ItemType>::operator=(HashMap<KeyType, ItemType>&& otherMap)
    {
        if (this != &otherMap)
        {
            for (uint32_t i = 0; i < m_numEntries; ++i)
            {
                m_entries[i].Clear();
            }
            Free(m_entries);

            m_entries = otherMap.m_entries;
            m_numEntries = otherMap.m_numEntries;
            m_maxEntries = otherMap.m_maxEntries;

            otherMap.m_entries = nullptr;
            otherMap.m_numEntries = 0;
            otherMap.m_maxEntries = 0;
        }
        return *this;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Reset()
    {
        for (uint32_t i = 0; i < m_numEntries; ++i)
        {
            m_entries[i].Clear();
        }
        Free(m_entries);

        m_entries = nullptr;
        m_numEntries = 0;
        m_maxEntries = 0;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Resize(uint32_t maxEntries)
    {
        if (m_maxEntries < maxEntries)
            ResizeBuckets(maxEntries);
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Optimize()
    {
        if (m_numEntries != m_maxEntries)
        {
            if (m_numEntries)
            {
                ResizeBuckets(m_numEntries);
            }
            else
            {
                Reset();
            }
        }
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::Swap(HashMap& otherMap)
    {
        std::swap(m_entries, otherMap.m_entries);
        std::swap(m_numEntries, otherMap.m_numEntries);
        std::swap(m_maxEntries, otherMap.m_maxEntries);
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::Iterator HashMap<KeyType, ItemType>::begin() const
    {
        return Iterator(*this);
    }

    template <class KeyType, class ItemType>
    inline HashMap<KeyType, ItemType>::Iterator HashMap<KeyType, ItemType>::end() const
    {
        return Iterator(*this, m_numEntries);
    }

    template <class KeyType, class ItemType>
    inline uint32_t* HashMap<KeyType, ItemType>::NextArray() const
    {
        return reinterpret_cast<uint32_t*>(m_entries + m_maxEntries);
    }

    template <class KeyType, class ItemType>
    inline uint32_t* HashMap<KeyType, ItemType>::BucketArray() const
    {
        return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(m_entries) + m_maxEntries * (sizeof(Entry) + sizeof(uint32_t)));
    }

    template <class KeyType, class ItemType>
    inline uint32_t HashMap<KeyType, ItemType>::BucketArraySize() const
    {
        return m_maxEntries * 2 - 1;
    }

    template <class KeyType, class ItemType>
    inline uint32_t HashMap<KeyType, ItemType>::CalcNewArraySize() const
    {
        return (m_numEntries >= m_maxEntries) ? (m_maxEntries * 2 + 1) : m_maxEntries;
    }

    template <class KeyType, class ItemType>
    inline uint32_t HashMap<KeyType, ItemType>::GetBucketIndex(uint32_t hash, uint32_t bucketArraySize)
    {
        return hash % bucketArraySize;
    }

    template <class KeyType, class ItemType>
    inline uint32_t HashMap<KeyType, ItemType>::GetBucketIndex(uint32_t hash) const
    {
        return GetBucketIndex(hash, BucketArraySize());
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::ResizeBuckets(uint32_t newMaxEntries)
    {
        const uint32_t newBucketArraySize = newMaxEntries * 2 - 1;

        auto pBuffer = Alloc<uint8_t>(newMaxEntries * (sizeof(Entry) + sizeof(uint32_t)) + newBucketArraySize * sizeof(uint32_t), alignof(Entry));
        Entry* pNewEntries = reinterpret_cast<Entry*>(pBuffer);
        uint32_t* pNewNextArray = reinterpret_cast<uint32_t*>(pBuffer + newMaxEntries * sizeof(Entry));
        uint32_t* pNewBucketArray = reinterpret_cast<uint32_t*>(pBuffer + newMaxEntries * (sizeof(Entry) + sizeof(uint32_t)));

        memset(pNewBucketArray, 0xFF, newBucketArraySize * sizeof(uint32_t));

        for (uint32_t readIndex = 0; readIndex < m_numEntries; ++readIndex)
        {
            const uint32_t newBucketIndex = GetBucketIndex(Hash::Get(*m_entries[readIndex].m_key()), newBucketArraySize);

            m_entries[readIndex].RelocateTo(pNewEntries[readIndex]);
            pNewNextArray[readIndex] = pNewBucketArray[newBucketIndex];
            pNewBucketArray[newBucketIndex] = readIndex;
        }

        Free(m_entries);

        m_entries = pNewEntries;
        m_maxEntries = newMaxEntries;
    }

    template <class KeyType, class ItemType>
    inline uint32_t HashMap<KeyType, ItemType>::FindIndexByKey(KeyParamType key) const
    {
        if (m_numEntries)
        {
            uint32_t bucketIndex = GetBucketIndex(Hash::Get(key));
            uint32_t* pNext = NextArray();
            for (uint32_t entryIndex = BucketArray()[bucketIndex]; entryIndex < m_numEntries; entryIndex = pNext[entryIndex])
            {
                if (Hash::Compare(key, *m_entries[entryIndex].m_key()))
                {
                    return entryIndex;
                }
            }
        }
        return kInvalidIndex;
    }

    template <class KeyType, class ItemType>
    inline void HashMap<KeyType, ItemType>::RemoveAtIndex(uint32_t index)
    {
        const uint32_t lastEntry = m_numEntries - 1;
        if (lastEntry != index)
        {
            // Fill in the gap and update "next" pointers
            const uint32_t bucketIndex = GetBucketIndex(Hash::Get(*m_entries[lastEntry].m_key()));
            uint32_t* pNext = NextArray();
            uint32_t* pBucketArray = BucketArray();
            bool lastEntryListUpdated = false;
            if (pBucketArray[bucketIndex] == lastEntry)
            {
                pBucketArray[bucketIndex] = index;
                lastEntryListUpdated = true;
            }
            else
            {
                for (uint32_t i = pBucketArray[bucketIndex]; i < m_numEntries; i = pNext[i])
                {
                    if (pNext[i] == lastEntry)
                    {
                        pNext[i] = index;
                        lastEntryListUpdated = true;
                        break;
                    }
                }
            }
            assert(lastEntryListUpdated && "Couldn't find the last entry"/*, hash map is corrupt.Key: % s", HashMapKeyAsString<KeyType>(*m_entries[lastEntry].m_key())*/);
            (void)lastEntryListUpdated;
            m_entries[index].Clear();
            m_entries[lastEntry].RelocateTo(m_entries[index]);
            pNext[index] = pNext[lastEntry];
        }
        else
        {
            m_entries[lastEntry].Clear();
        }
        m_numEntries = lastEntry;
    }

    template <class KeyType, class ItemType>
    template <typename T>
    inline const char* HashMap<KeyType, ItemType>::HashMapKeyAsString(const T&)
    {
        return "not a string";
    }

    template <class KeyType, class ItemType>
    inline const char* HashMap<KeyType, ItemType>::HashMapKeyAsString(const char* const& key)
    {
        return key;
    }
}
// namespace core