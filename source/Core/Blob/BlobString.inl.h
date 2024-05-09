#pragma once

#include "BlobString.h"
#include <Blob/BlobSerializer.h>

namespace core
{
    template <Blob::Storage storage>
    inline BlobString<storage>::BlobString(const BlobString<Blob::kIsStatic>& otherBlobString) requires (storage == Blob::kIsDynamic)
        : m_handle(otherBlobString.Items())
    {}

    template <Blob::Storage storage>
    inline BlobString<storage>::BlobString(const BlobString<Blob::kIsDynamic>& otherBlobString) requires (storage == Blob::kIsDynamic)
        : m_handle(otherBlobString.m_handle)
    {}

    template <Blob::Storage storage>
    inline BlobString<storage>::BlobString(const char* otherString) requires (storage == Blob::kIsDynamic)
        : m_handle(otherString)
    {}

    template <Blob::Storage storage>
    inline BlobString<storage>::BlobString(std::string&& otherString) requires (storage == Blob::kIsDynamic)
        : m_handle(std::move(otherString))
    {}

    template <Blob::Storage storage>
    inline bool BlobString<storage>::IsEmpty() const
    {
        return Items() == nullptr || Items()[0] == 0;
    }

    template <Blob::Storage storage>
    inline bool BlobString<storage>::IsNotEmpty() const
    {
        return Items() != nullptr && Items()[0] != 0;
    }

    template <Blob::Storage storage>
    inline bool BlobString<storage>::operator==(const char* otherString) const
    {
        return strcmp(Items(), otherString) == 0;
    }

    template <Blob::Storage storage>
    inline bool BlobString<storage>::operator==(const std::string& otherString) const
    {
        return (*this) == otherString.c_str();
    }

    template <Blob::Storage storage>
    template <Blob::Storage otherStorage>
    inline bool BlobString<storage>::operator==(const BlobString<otherStorage>& otherBlobString) const
    {
        return strcmp(Items(), otherBlobString.Items()) == 0;
    }

    template <Blob::Storage storage>
    inline const char* BlobString<storage>::Items() const
    {
        const char* str;
        if constexpr (storage == Blob::kIsStatic)
            str = reinterpret_cast<const char*>(this) + m_handle.offset;
        else
            str = m_handle.c_str();
        return str;
    }

    template <Blob::Storage storage>
    inline std::string& BlobString<storage>::String() requires (storage == Blob::kIsDynamic)
    {
        return m_handle;
    }

    template <Blob::Storage storage>
    inline BlobString<storage>& BlobString<storage>::operator=(const std::string& otherString) requires (storage == Blob::kIsDynamic)
    {
        m_handle = otherString;
        return *this;
    }

    template <Blob::Storage storage>
    inline BlobString<storage>& BlobString<storage>::operator=(std::string&& otherString) requires (storage == Blob::kIsDynamic)
    {
        m_handle = std::move(otherString);
        return *this;
    }

    template <Blob::Storage storage>
    inline BlobString<storage>& BlobString<storage>::operator=(const BlobString<Blob::kIsStatic>& otherBlobString) requires (storage == Blob::kIsDynamic)
    {
        m_handle = otherBlobString.Items();
        return *this;
    }

    template <Blob::Storage storage>
    inline BlobString<storage>& BlobString<storage>::operator=(const BlobString<Blob::kIsDynamic>& otherBlobString) requires (storage == Blob::kIsDynamic)
    {
        m_handle = otherBlobString.m_handle;
        return *this;
    }

    template <Blob::Storage storage>
    inline void BlobString<storage>::Store(BlobSerializer& s) const requires (storage == Blob::kIsDynamic)
    {
        s.Store(StaticInfo(0));
        if (!m_handle.empty() && *m_handle.c_str() != 0)
        {
            s.Push(sizeof(StaticInfo));
            s.Store(m_handle.c_str(), uint32_t(m_handle.size() + 1));
            s.Pop();
        }
    }

    template <Blob::Storage storage>
    inline void BlobString<storage>::Load(io::File& file) requires (storage == Blob::kIsDynamic)
    {
        file.Read(m_handle);
    }

    template <Blob::Storage storage>
    inline void BlobString<storage>::Save(io::File& file) const requires (storage == Blob::kIsDynamic)
    {
        file.Write(m_handle);
    }
}
// namespace core