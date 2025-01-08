#include <Containers/Array.h>

#include "File.h"

#include <windows.h>

namespace core::io
{
    File File::OpenForRead(const char* name)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
        auto wName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, numChars);

        return OpenForRead(wName);
    }

    File File::OpenForWrite(const char* name)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
        auto wName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, numChars);

        return OpenForWrite(wName);
    }

    File File::OpenForAppend(const char* name)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
        auto wName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, numChars);

        return OpenForAppend(wName);
    }

    bool File::IsExisting(const char* name)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
        auto wName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, numChars);
        return IsExisting(wName);
    }

    File File::OpenForRead(const wchar_t* name)
    {
        DWORD dwDesiredAccess = GENERIC_READ;
        DWORD dwShareMode = FILE_SHARE_READ;
        DWORD dwCreationDisposition = OPEN_EXISTING;
        DWORD dwFlagsAndAttributes = 0;

        return ::CreateFileW(name, dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, dwFlagsAndAttributes, NULL);
    }

    File File::OpenForWrite(const wchar_t* name)
    {
        CreateDirectories(name, true);

        DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
        DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        DWORD dwCreationDisposition = CREATE_ALWAYS;
        DWORD dwFlagsAndAttributes = 0;

        return ::CreateFileW(name, dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, dwFlagsAndAttributes, NULL);
    }

    File File::OpenForAppend(const wchar_t* name)
    {
        CreateDirectories(name, true);

        DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
        DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
        DWORD dwCreationDisposition = OPEN_ALWAYS;
        DWORD dwFlagsAndAttributes = 0;

        File file = ::CreateFileW(name, dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, dwFlagsAndAttributes, NULL);
        file.Seek(file.GetSize());
        return file;
    }

    bool File::IsExisting(const wchar_t* name)
    {
        WIN32_FIND_DATAW findData;
        auto handle = ::FindFirstFileW(name, &findData);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        FindClose(handle);
        return true;
    }

    File::~File()
    {
        ::CloseHandle(mHandle);
    }

    File::File(File&& other)
    {
        mHandle = other.mHandle;
        other.mHandle = INVALID_HANDLE_VALUE;
    }

    File& File::operator=(File&& other)
    {
        ::CloseHandle(mHandle);
        mHandle = other.mHandle;
        other.mHandle = INVALID_HANDLE_VALUE;
        return *this;
    }

    uint64_t File::GetSize() const
    {
        LARGE_INTEGER llFileSize = { 0, 0 };

        FILE_STANDARD_INFO fileStandardInfo;
        if (::GetFileInformationByHandleEx(mHandle, FileStandardInfo, &fileStandardInfo, sizeof(FILE_STANDARD_INFO)))
        {
            llFileSize = fileStandardInfo.EndOfFile;
        }

        return llFileSize.QuadPart;
    }

    uint64_t File::Read(void* buffer, uint64_t size) const
    {
        DWORD dwBytesRead = 0;
        ::ReadFile(mHandle, buffer, uint32_t(size), &dwBytesRead, nullptr);

        return dwBytesRead;
    }

    void File::Write(const void* buffer, uint64_t size) const
    {
        DWORD dwBytesWritten = 0;
        ::WriteFile(mHandle, buffer, uint32_t(size), &dwBytesWritten, nullptr);
    }

    uint64_t File::GetPosition() const
    {
        LARGE_INTEGER largeOffset{ 0 };
        LARGE_INTEGER newPos{ 0 };
        ::SetFilePointerEx(mHandle, largeOffset, &newPos, FILE_CURRENT);
        return newPos.QuadPart;
    }

    void File::Seek(uint64_t offset)
    {
        LARGE_INTEGER largeOffset;
        largeOffset.QuadPart = offset;
        ::SetFilePointerEx(mHandle, largeOffset, nullptr, FILE_BEGIN);
    }

    bool File::Delete(const wchar_t* name)
    {
        auto e = ::DeleteFileW(name);
        auto error = GetLastError();
        if (e || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            DeleteDirectories(name);
            e = TRUE;
        }
        return e;
    }

    bool File::Delete(const char* name)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
        auto wName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, numChars);

        return Delete(wName);
    }

    bool File::Copy(const char* srcName, const char* dstName)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, srcName, -1, NULL, 0);
        auto wSrcName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, srcName, -1, wSrcName, numChars);

        numChars = ::MultiByteToWideChar(CP_UTF8, 0, dstName, -1, NULL, 0);
        auto wDstName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, dstName, -1, wDstName, numChars);

        CreateDirectories(wDstName, true);
        auto e = ::CopyFileW(wSrcName, wDstName, FALSE);
        if (e == 0)
            DeleteDirectories(wDstName);
        return e != 0;
    }

    bool File::Move(const char* oldName, const char* newName)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, oldName, -1, NULL, 0);
        auto wOldName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, oldName, -1, wOldName, numChars);

        numChars = ::MultiByteToWideChar(CP_UTF8, 0, newName, -1, NULL, 0);
        auto wNewName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, newName, -1, wNewName, numChars);

        CreateDirectories(wNewName, true);
        auto e = ::MoveFileW(wOldName, wNewName);
        if (e == 0)
            DeleteDirectories(wNewName);
        else
            DeleteDirectories(wOldName);
        return e != 0;
    }

    bool File::Rename(const char* oldName, const char* newName)
    {
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, oldName, -1, NULL, 0);
        auto wOldName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, oldName, -1, wOldName, numChars);

        numChars = ::MultiByteToWideChar(CP_UTF8, 0, newName, -1, NULL, 0);
        auto wNewName = reinterpret_cast<wchar_t*>(_alloca(numChars * sizeof(wchar_t)));
        ::MultiByteToWideChar(CP_UTF8, 0, newName, -1, wNewName, numChars);

        return ::MoveFileW(wOldName, wNewName);
    }

    void File::CleanFilename(char* name)
    {
        while (auto c = *name)
        {
            if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' || (c >= 1 && c <= 31))
                *name = '_';
            ++name;
        }
    }

    std::wstring File::Convert(const char* name)
    {
        std::wstring wName;
        auto numChars = ::MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
        wName.resize(numChars);
        ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wName.data(), numChars);
        return wName;
    }

    File::File(void* handle)
        : mHandle(handle)
    {}

    void File::CreateDirectories(const wchar_t* name, bool isFile)
    {
        std::wstring n(name);
        bool isDirectoryCreated = true;
        for (auto str = n.data(); *str; str++)
        {
            auto c = *str;
            if (c == L'/' || c == L'\\')
            {
                if (!isDirectoryCreated)
                {
                    *str = 0;
                    ::CreateDirectoryW(n.data(), nullptr);
                    *str = c;
                    isDirectoryCreated = true;
                }
            }
            else
                isDirectoryCreated = false;
        }
        if (!isDirectoryCreated && !isFile)
            ::CreateDirectoryW(n.data(), nullptr);
    }

    void File::DeleteDirectories(const wchar_t* name)
    {
        std::wstring n(name);
        for (auto e = n.data(), str = e + n.size() - 1; str != e; str--)
        {
            auto c = *str;
            if (c == L'/' || c == L'\\')
            {
                *str = 0;
                if (::RemoveDirectoryW(n.data()) == 0)
                    return;
            }
        }
        ::RemoveDirectoryW(n.data());
    }
}
// namespace core::io