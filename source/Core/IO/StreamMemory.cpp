#include "StreamFile.h"
#include "StreamMemory.h"

#include <Core.h>

#include <filesystem>

namespace core::io
{
    SmartPtr<StreamMemory> StreamMemory::Create(const std::string& filename, const uint8_t* buffer, uint64_t size, bool isStatic)
    {
        SmartPtr<StreamMemory> stream;
        stream.New();
        stream->m_buffer = buffer;
        stream->m_size = size;
        stream->m_name = filename;
        if (!isStatic)
            stream->m_mem.New(const_cast<uint8_t*>(buffer));
        return stream;
    }

    SmartPtr<StreamMemory> StreamMemory::Create(const std::string& filename, SharedMemory* sharedMemory, uint64_t size)
    {
        SmartPtr<StreamMemory> stream;
        stream.New();
        stream->m_buffer = reinterpret_cast<const uint8_t*>(sharedMemory->m_ptr);
        stream->m_size = size;
        stream->m_name = filename;
        stream->m_mem = sharedMemory;
        return stream;
    }

    uint64_t StreamMemory::Read(void* buffer, uint64_t size)
    {
        auto remainingSize = m_size - m_position;
        size = Min(remainingSize, size);
        if (size > 0)
        {
            memcpy(buffer, m_buffer + m_position, size_t(size));
            m_position += size;
        }
        return size;
    }

    Status StreamMemory::Seek(int64_t offset, SeekWhence whence)
    {
        if (whence == SeekWhence::kSeekCurrent)
            offset += m_position;
        else if (whence == SeekWhence::kSeekEnd)
            offset += m_size;
        if (offset < 0 || offset > int64_t(m_size))
            return Status::kFail;
        m_position = offset;
        return Status::kOk;
    }

    uint64_t StreamMemory::GetSize() const
    {
        return m_size;
    }

    uint64_t StreamMemory::GetPosition() const
    {
        return uint64_t(m_position);
    }

    SmartPtr<Stream> StreamMemory::Open(const std::string& filename)
    {
        std::filesystem::path path(m_name);
        path.replace_filename(filename);
        return StreamFile::Create(path.string());
    }

    SmartPtr<Stream> StreamMemory::OnClone()
    {
        auto stream = Create(m_name, m_mem, m_size);
        return static_cast<SmartPtr<Stream>>(stream);
    }

    const Span<const uint8_t> StreamMemory::Read()
    {
        m_position = int64_t(m_size);
        return { m_buffer, uint32_t(m_size) };
    }
}
// namespace core::io