#pragma once

//#include <ls_common/containers/HashTypes.h>

#include <Core.h>

namespace core
{
    template <class T>
    struct HashMapKeyStorage
    {
        typedef T Type;
        typedef const T& ParamType;
        typedef const T& ReturnType;
    };

/*
    template <>
    struct HashMapKeyStorage<std::string>
    {
        typedef std::string::string Type;
        typedef const char* ParamType;
        typedef const std::string::string& ReturnType;
    };

    template <>
    struct HashMapKeyStorage<const char*>
    {
        typedef std::string::string Type;
        typedef const char* ParamType;
        typedef const char* ReturnType;
    };
*/

    template <class KeyType, class ItemType>
    class HashMap
    {
        typedef typename HashMapKeyStorage<KeyType>::Type KeyStorageType;
        typedef typename HashMapKeyStorage<KeyType>::ParamType KeyParamType;
        typedef typename HashMapKeyStorage<KeyType>::ReturnType KeyReturnType;

        struct Entry
        {
            void Set(KeyParamType key, const ItemType& item);
            void MoveSet(KeyParamType key, ItemType&& item);
            void Set(KeyParamType key);
            void MoveSet(KeyType&& key);
            void Clear();
            void RelocateTo(Entry& entry);

            template <typename T>
            struct Storage
            {
                T* operator()();
                const T* operator()() const;

                alignas(alignof(T)) uint8_t m_buffer[sizeof(T)];
            };

            Storage<KeyStorageType> m_key;
            Storage<ItemType> m_item;
        };

    public:
        class Iterator
        {
            friend class HashMap;
        public:
            Iterator();
            Iterator(const HashMap& map);
            Iterator(const HashMap& map, uint32_t index);

            bool operator==(const Iterator& otherIt) const;
            bool operator!=(const Iterator& otherIt) const;

            operator bool() const;
            KeyReturnType Key() const;
            KeyStorageType* KeyPtr() const;
            ItemType& Item();
            const ItemType& Item() const;
            ItemType* operator->();
            const ItemType* operator->() const;

            ItemType& operator*();
            const ItemType& operator*() const;

            // ++ and -- operators returns void to avoid prefix/postfix problems.
            void operator++();      // prefix
            void operator++(int);   // postfix
            void operator--();      // prefix
            void operator--(int);   // postfix

        private:
            const HashMap* m_map;
            uint32_t m_index;
        };

    public:
        // Setup
        explicit HashMap(uint32_t maxEntries = 0);
        HashMap(const HashMap& otherMap);
        HashMap(HashMap&& otherMap);
        ~HashMap();

        // Accessors
        uint32_t NumItems() const;

        ItemType& operator[](KeyParamType key);
        ItemType& operator[](KeyStorageType&& key);
        const ItemType& operator[](KeyParamType key) const;

//         KeyReturnType GetKeyByIndex(uint32_t index) const;
//         ItemType& GetItemByIndex(uint32_t index) const;

        const ItemType& Get(KeyParamType key, const ItemType& defaultItem) const;

        ItemType* const FindItemByKey(KeyParamType key) const;
        bool FindKeyByItem(const ItemType& item, KeyType& key) const;

        // Modifiers
        bool PopByKey(KeyParamType key, ItemType& item);

        ItemType& Insert(KeyParamType key, const ItemType& item);
        ItemType& Insert(KeyParamType key, ItemType&& item);

        template <typename RunItem>
        bool RemoveByKey(KeyParamType key, RunItem runItem);
        bool RemoveByKey(KeyParamType key);
        bool RemoveByItem(const ItemType& item);
        bool RemoveByHashAndItem(uint32_t hash, const ItemType& item);
        void RemoveAndStepIteratorForward(Iterator& iterator);
        void RemoveAll();
        template <typename Releaser>
        void ReleaseAll(Releaser releaser);
        void DeleteAll();

        HashMap& operator=(const HashMap& otherMap);
        HashMap& operator=(HashMap&& otherMap);

        // Storage
        void Reset();
        void Resize(uint32_t maxEntries);
        void Optimize();

        void Swap(HashMap& otherMap);

        // stl
        Iterator begin() const;
        Iterator end() const;

        static constexpr uint32_t kInvalidIndex = 0xffFFffFF;

    private:
        uint32_t* NextArray() const;
        uint32_t* BucketArray() const;
        uint32_t BucketArraySize() const;
        uint32_t CalcNewArraySize() const;
        static uint32_t GetBucketIndex(uint32_t hash, uint32_t bucketArraySize);
        uint32_t GetBucketIndex(uint32_t hash) const;
        void ResizeBuckets(uint32_t newMaxEntries);

        uint32_t FindIndexByKey(KeyParamType key) const;
        void RemoveAtIndex(uint32_t index);

        template <typename T>
        static const char* HashMapKeyAsString(const T&);
        static const char* HashMapKeyAsString(const char* const& key);

    private:
        Entry* m_entries;
        uint32_t m_numEntries;
        uint32_t m_maxEntries;
    };
}
// namespace core

#include "HashMap.inl.h"