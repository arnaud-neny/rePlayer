#include "StreamFile.h"
#include "StreamMemory.h"

#include <Core.h>

#include <filesystem>

namespace core::io
{
    SmartPtr<StreamMemory> StreamMemory::Create(const std::string& filename, const uint8_t* buffer, size_t size, bool isStatic)
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

    size_t StreamMemory::Read(void* buffer, size_t size)
    {
        auto remainingSize = m_size - m_position;
        size = Min(remainingSize, size);
        if (size > 0)
        {
            memcpy(buffer, m_buffer + m_position, size);
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

    size_t StreamMemory::GetSize() const
    {
        return m_size;
    }

    size_t StreamMemory::GetPosition() const
    {
        return size_t(m_position);
    }

    SmartPtr<Stream> StreamMemory::Open(const std::string& filename)
    {
        std::filesystem::path path(m_name);
        path.replace_filename(filename);
        return StreamFile::Create(path.string());
    }

    SmartPtr<Stream> StreamMemory::OnClone()
    {
        auto stream = Create(m_name, m_buffer, m_size, true);
        stream->m_mem = m_mem;
        return static_cast<SmartPtr<Stream>>(stream);
    }

    const Span<const uint8_t> StreamMemory::Read()
    {
        m_position = int64_t(m_size);
        return { m_buffer, m_size };
    }
}
// namespace core::io