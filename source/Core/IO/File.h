#pragma once

#include <Core.h>
#include <Containers/Array.h>

#include <string>

namespace core::io
{
    class File
    {
    public:
        static File OpenForRead(const char* name);
        static File OpenForWrite(const char* name);
        static File OpenForAppend(const char* name);
        static bool IsExisting(const char* name);

        static File OpenForRead(const wchar_t* name);
        static File OpenForWrite(const wchar_t* name);
        static File OpenForAppend(const wchar_t* name);
        static bool IsExisting(const wchar_t* name);

        static bool Delete(const wchar_t* name);

        File() = default;
        ~File();

        File(File&& other);
        File& operator=(File&& other);

        bool IsValid() const { return mHandle != (void*)-1ll; }
        uint64_t GetSize() const;

        uint64_t Read(void* buffer, uint64_t size) const;
        template <typename Type>
        uint64_t Read(Type& data) const { return Read(&data, sizeof(data)); }
        template <typename Type>
        Type Read() const { Type data = {}; Read(&data, sizeof(data)); return data; }
        template <typename SizeType, typename Type>
        void Read(Array<Type>& data) const { auto numItems = Read<SizeType>(); data.Resize(numItems); if (numItems > 0) Read(data.begin(), data.Size()); }
        void Read(std::string& data) const { auto size = Read<uint16_t>(); data.resize(size); if (size > 0) Read(data.data(), size); }
        template <typename SizeType>
        void Read(Array<std::string>& data) const { auto numItems = Read<SizeType>(); data.Resize(numItems); for (auto& element : data) Read(element); }

        void Write(const void* buffer, uint64_t size) const;
        template <typename Type>
        void Write(const Type& data) const { Write(&data, sizeof(data)); }
        template <typename DataType, typename ValueType>
        void WriteAs(const ValueType& value) const { auto data = static_cast<DataType>(value); Write(&data, sizeof(data)); }
        template <typename SizeType, typename Type>
        void Write(const Array<Type>& data) const { WriteAs<SizeType>(data.NumItems()); if (data.IsNotEmpty()) Write(data.begin(), data.Size()); }
        void Write(const std::string& data) const { WriteAs<uint16_t>(data.size()); if (!data.empty()) Write(data.data(), data.size()); }
        template <typename SizeType>
        void Write(const Array<std::string>& data) const { WriteAs<SizeType>(data.NumItems()); for (auto& element : data) Write(element); }

        uint64_t GetPosition() const;
        void Seek(uint64_t offset);

        static bool Delete(const char* name);
        static bool Copy(const char* srcName, const char* dstName);
        static bool Move(const char* oldName, const char* newName);
        static bool Rename(const char* oldName, const char* newName);

        //replace invalid characters with underscores
        static void CleanFilename(char* name);

        static std::wstring Convert(const char* name);

    private:
        File(void* handle);
        File(const File&) = delete;

        File& operator=(const File&) = delete;

        static void CreateDirectories(const wchar_t* name, bool isFile);
        static void DeleteDirectories(const wchar_t* name);

    private:
        void* mHandle = (void*)-1ll;
    };
}
// namespace core::io