#pragma once

#include <Blob/Blob.h>
#include <Blob/BlobSerializer.h>
#include <IO/File.h>
#include <string>

namespace core
{
    /*
    * Blob::kIsStatic: handle is an offset from this to the nul terminated string
    * Blob::kIsDynamic: handle is a std::string
    */
    template <Blob::Storage storage>
    class BlobString
    {
        struct StaticInfo
        {
            uint16_t offset;
        };
        typedef std::conditional<storage == Blob::kIsStatic, StaticInfo, std::string>::type Type;
        template <Blob::Storage otherStorage>
        friend class BlobString;
    public:
        // Setup
        BlobString() = default;
        BlobString(const BlobString<Blob::kIsStatic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobString(const BlobString<Blob::kIsDynamic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobString(const BlobString<Blob::kIsStatic>& otherBlobString) requires (storage == Blob::kIsDynamic);
        BlobString(const BlobString<Blob::kIsDynamic>& otherBlobString) requires (storage == Blob::kIsDynamic);
        BlobString(const char* otherString) requires (storage == Blob::kIsDynamic);
        BlobString(std::string&& otherString) requires (storage == Blob::kIsDynamic);
        ~BlobString() = default;

        // States
        bool IsEmpty() const;
        bool IsNotEmpty() const;

        bool operator==(const char* otherString) const;
        bool operator==(const std::string& otherString) const;
        template <Blob::Storage otherStorage>
        bool operator==(const BlobString<otherStorage>& otherBlobString) const;

        // Accessors
        const char* Items() const;
        std::string& String() requires (storage == Blob::kIsDynamic);

        // Modifiers
        BlobString& operator=(const std::string& otherString) requires (storage == Blob::kIsDynamic);
        BlobString& operator=(std::string&& otherString) requires (storage == Blob::kIsDynamic);
        BlobString& operator=(const BlobString<Blob::kIsStatic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobString& operator=(const BlobString<Blob::kIsDynamic>&) requires (storage == Blob::kIsStatic) = delete;
        BlobString& operator=(const BlobString<Blob::kIsStatic>& otherBlobString) requires (storage == Blob::kIsDynamic);
        BlobString& operator=(const BlobString<Blob::kIsDynamic>& otherBlobString) requires (storage == Blob::kIsDynamic);

        // Serialization
        void Store(BlobSerializer& s) const requires (storage == Blob::kIsDynamic);
        void Load(io::File& file) requires (storage == Blob::kIsDynamic);
        void Save(io::File& file) const requires (storage == Blob::kIsDynamic);

    private:
        Type m_handle = {};
    };
}
// namespace core

#include "BlobString.inl.h"