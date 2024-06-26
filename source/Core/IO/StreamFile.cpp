#include "StreamFile.h"
#include "StreamMemory.h"

#include <windows.h>

#include <filesystem>

namespace core::io
{
    SmartPtr<StreamFile> StreamFile::Create(const std::string& filename, Stream* root)
    {
        SmartPtr<StreamFile> stream;
        auto handle = ::CreateFileW(Convert(filename).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle != INVALID_HANDLE_VALUE)
        {
            stream.New(root);
            stream->m_handle = handle;
            stream->m_name = filename;
            stream->AddFilename(filename);
        }
        return stream;
    }

    SmartPtr<StreamFile> StreamFile::Create(const std::wstring& filename, Stream* root)
    {
        SmartPtr<StreamFile> stream;
        auto handle = ::CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle != INVALID_HANDLE_VALUE)
        {
            stream.New(root);
            stream->m_handle = handle;
            stream->m_name = Convert(filename);
            stream->AddFilename(stream->m_name);
        }
        return stream;
    }

    uint64_t StreamFile::Read(void* buffer, uint64_t size)
    {
        if (m_stream.IsValid())
            return m_stream->Read(buffer, size);
        uint64_t totalRead = 0;
        while (totalRead < size)
        {
            uint32_t toRead = size > 0xffFFffFF ? 0xffFFffFF : uint32_t(size);
            DWORD readSize = 0;
            ::ReadFile(m_handle, buffer, uint32_t(size), &readSize, nullptr);
            totalRead += readSize;
            if (toRead != readSize)
                break;
        }
        return totalRead;
    }

    Status StreamFile::Seek(int64_t offset, SeekWhence whence)
    {
        if (m_stream.IsValid())
            return m_stream->Seek(offset, whence);
        LARGE_INTEGER seekPos;
        seekPos.QuadPart = offset;
        LARGE_INTEGER newPos = { 0 };
        if (::SetFilePointerEx(m_handle, seekPos, &newPos, whence == SeekWhence::kSeekCurrent ? FILE_CURRENT : whence == SeekWhence::kSeekBegin ? FILE_BEGIN : FILE_END))
            return Status::kOk;
        return Status::kFail;
    }

    uint64_t StreamFile::GetSize() const
    {
        if (m_stream.IsValid())
            return m_stream->GetSize();

        LARGE_INTEGER llFileSize{ 0, 0 };

        FILE_STANDARD_INFO fileStandardInfo;
        if (::GetFileInformationByHandleEx(m_handle, FileStandardInfo, &fileStandardInfo, sizeof(FILE_STANDARD_INFO)))
        {
            llFileSize = fileStandardInfo.EndOfFile;
        }

        return llFileSize.QuadPart;
    }

    uint64_t StreamFile::GetPosition() const
    {
        if (m_stream.IsValid())
            return m_stream->GetPosition();

        LARGE_INTEGER seekPos{ 0 };
        LARGE_INTEGER newPos{ 0 };
        ::SetFilePointerEx(m_handle, seekPos, &newPos, FILE_CURRENT);
        return newPos.QuadPart;
    }

    StreamFile::StreamFile(Stream* root)
        : Stream(root)
    {}

    StreamFile::~StreamFile()
    {
        if (m_handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(m_handle);
    }

    const Span<const uint8_t> StreamFile::Read()
    {
        if (m_stream.IsValid())
            return m_stream->Read();

        auto data = Stream::Read();
        ::CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;

        m_stream = StreamMemory::Create(m_name, m_cachedData, data.Size(), nullptr);
        return data;
    }

    SmartPtr<Stream> StreamFile::OnOpen(const std::string& filename)
    {
        std::filesystem::path path(m_name);
        path.replace_filename(filename);
        auto stream = Create(path.string(), GetRoot());
        if (stream.IsInvalid())
            stream = Create(filename, GetRoot());
        return stream;
    }

    SmartPtr<Stream> StreamFile::OnClone()
    {
        if (m_stream.IsValid())
            return m_stream->Clone();
        return Create(m_name, GetRoot());
    }

    std::wstring StreamFile::Convert(const std::string& name)
    {
        std::wstring wName;
        wName.resize(::MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0));
        ::MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), int32_t(wName.size()));
        return wName;
    }

    std::string StreamFile::Convert(const std::wstring& wName)
    {
        std::string name;
        name.resize(::WideCharToMultiByte(CP_UTF8, 0, wName.c_str(), -1, nullptr, 0, nullptr, nullptr));
        ::WideCharToMultiByte(CP_UTF8, 0, wName.c_str(), -1, name.data(), int32_t(name.size()), nullptr, nullptr);
        return name;
    }
}
// namespace core::io