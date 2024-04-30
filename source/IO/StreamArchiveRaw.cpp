// Core
// #include <Core/Log.h>
#include <Core/String.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>

// rePlayer
#include "StreamArchiveRaw.h"

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

namespace rePlayer
{
    SmartPtr<StreamArchiveRaw> StreamArchiveRaw::Create(io::Stream* stream)
    {
        if (stream)
        {
            SmartPtr<StreamArchiveRaw> archiveStream(kAllocate, stream);
            if (archiveStream->m_stream)
            {
                if (archive_entry_size_is_set(archiveStream->m_entry))
                {
                    archiveStream->m_entrySize = uint32_t(archive_entry_size(archiveStream->m_entry));
                }
                else
                {
                    la_int64_t dataBlockOffset;
                    while (archive_read_data_block(archiveStream->m_archive, (const void**)&archiveStream->m_entryDataBlock, &archiveStream->m_entryDataBlockSize, &dataBlockOffset) == ARCHIVE_OK)
                        archiveStream->m_entrySize += archiveStream->m_entryDataBlockSize;
                    archiveStream->ReOpen();
                }
                return archiveStream;
            }
        }
        return nullptr;
    }

    size_t StreamArchiveRaw::Read(void* buffer, size_t size)
    {
        if (m_streamMemory.IsValid())
            return m_streamMemory->Read(buffer, size);

        auto* dest = reinterpret_cast<uint8_t*>(buffer);
        auto sizeLeft = int64_t(size);
        while (sizeLeft > 0)
        {
            auto remainingSize = int64_t(m_entryDataBlockSize) - m_entryDataBlockPosition;
            if (remainingSize == 0)
            {
                la_int64_t dataBlockOffset;
                if (archive_read_data_block(m_archive, (const void**)&m_entryDataBlock, &m_entryDataBlockSize, &dataBlockOffset) != ARCHIVE_OK)
                {
                    m_entryDataBlockSize = 0;
                    m_entryDataBlockPosition = 0;
                    return size - sizeLeft;
                }
                remainingSize = m_entryDataBlockSize;
                m_entryDataBlockPosition = 0;
            }
            auto sizeToCopy = Min(sizeLeft, remainingSize);
            memcpy(dest, m_entryDataBlock + m_entryDataBlockPosition, sizeToCopy);
            dest += sizeToCopy;
            m_entryPosition += sizeToCopy;
            m_entryDataBlockPosition += sizeToCopy;
            sizeLeft -= sizeToCopy;
        }

        return size - sizeLeft;
    }

    Status StreamArchiveRaw::Seek(int64_t offset, SeekWhence whence)
    {
        if (m_streamMemory.IsValid())
            return m_streamMemory->Seek(offset, whence);

        auto position = m_entryPosition;
        switch (whence)
        {
        case kSeekBegin:
            break;
        case kSeekCurrent:
            offset = position + offset;
            break;
        case kSeekEnd:
            offset = m_entrySize + offset;
        }
        if (position > offset)
        {
            ReOpen();
            position = 0;
        }

        while (position < offset)
        {
            auto remainingSize = int64_t(m_entryDataBlockSize) - m_entryDataBlockPosition;
            if (remainingSize == 0)
            {
                la_int64_t dataBlockOffset;
                if (archive_read_data_block(m_archive, (const void**)&m_entryDataBlock, &m_entryDataBlockSize, &dataBlockOffset) != ARCHIVE_OK)
                {
                    m_entryDataBlockSize = 0;
                    m_entryDataBlockPosition = 0;
                    m_entryPosition = position;
                    return Status::kFail;
                }
                remainingSize = m_entryDataBlockSize;
                m_entryDataBlockPosition = 0;
            }
            auto size = Min(offset - position, remainingSize);
            position += size;
            m_entryDataBlockPosition += size;
        }
        m_entryPosition = position;

        return Status::kOk;
    }

    size_t StreamArchiveRaw::GetSize() const
    {
        return m_entrySize;
    }

    size_t StreamArchiveRaw::GetPosition() const
    {
        if (m_streamMemory.IsValid())
            return m_streamMemory->GetPosition();
        return m_entryPosition;
    }

    const std::string& StreamArchiveRaw::GetName() const
    {
        return m_entryFilename;
    }

    SmartPtr<io::Stream> StreamArchiveRaw::Open(const std::string& filename)
    {
        auto stream = m_stream->Open(filename);
        if (stream.IsValid())
        {
            if (auto archiveStream = StreamArchiveRaw::Create(stream))
                stream = archiveStream;
        }
        return stream;
    }

    const Span<const uint8_t> StreamArchiveRaw::Read()
    {
        if (m_streamMemory.IsValid())
            return m_streamMemory->Read();

        auto data = Stream::Read();
        m_streamMemory = io::StreamMemory::Create(m_stream->GetName(), m_cachedData, data.Size());
        return data;
    }

    StreamArchiveRaw::StreamArchiveRaw(io::Stream* stream)
        : m_stream(stream)
        , m_archive(archive_read_new())
    {
        stream->Seek(0, kSeekBegin);

        archive_read_support_filter_all(m_archive);
//         archive_read_support_format_empty(m_archive);
        archive_read_support_format_raw(m_archive);


        archive_read_set_read_callback(m_archive, reinterpret_cast<archive_read_callback*>(ArchiveRead));
        archive_read_set_seek_callback(m_archive, reinterpret_cast<archive_seek_callback*>(ArchiveSeek));
        archive_read_set_skip_callback(m_archive, reinterpret_cast<archive_skip_callback*>(ArchiveSkip));
        archive_read_set_callback_data(m_archive, this);
        // TBD: I suppose a raw has only one entry
        if (archive_read_open1(m_archive) != ARCHIVE_OK || _stricmp(archive_filter_name(m_archive, 0), "none") == 0 || archive_read_next_header(m_archive, &m_entry) != ARCHIVE_OK)
            m_stream.Reset();
        else
            m_entryFilename = archive_entry_pathname(m_entry);
    }

    StreamArchiveRaw::~StreamArchiveRaw()
    {
        archive_read_free(m_archive);
    }

    SmartPtr<io::Stream> StreamArchiveRaw::OnClone()
    {
        SmartPtr<StreamArchiveRaw> stream(kAllocate, m_stream->Clone());
        if (stream->m_stream.IsValid())
        {
            if (m_streamMemory.IsValid())
                stream->m_streamMemory = m_streamMemory->Clone();
            archive_read_next_header(stream->m_archive, &stream->m_entry);
            stream->m_entrySize = m_entrySize;
            return stream;
        }
        return nullptr;
    }

    void StreamArchiveRaw::ReOpen()
    {
        archive_read_free(m_archive);

        m_archive = archive_read_new();

        archive_read_support_filter_all(m_archive);
//         archive_read_support_format_empty(m_archive);
        archive_read_support_format_raw(m_archive);

        archive_read_set_read_callback(m_archive, reinterpret_cast<archive_read_callback*>(ArchiveRead));
        archive_read_set_seek_callback(m_archive, reinterpret_cast<archive_seek_callback*>(ArchiveSeek));
        archive_read_set_skip_callback(m_archive, reinterpret_cast<archive_skip_callback*>(ArchiveSkip));
        archive_read_set_callback_data(m_archive, this);

        m_entryPosition = 0;
        m_entryDataBlockSize = 0;
        m_entryDataBlockPosition = 0;

        m_stream->Seek(0, kSeekBegin);
        archive_read_open1(m_archive);
        archive_read_next_header(m_archive, &m_entry);
    }

    int64_t StreamArchiveRaw::ArchiveRead(struct archive* a, StreamArchiveRaw* stream, const void** buf)
    {
        (void)a;
        *buf = stream->m_cache;
        return int64_t(stream->m_stream->Read(stream->m_cache, kCacheSize));
    }

    int64_t StreamArchiveRaw::ArchiveSeek(struct archive* a, StreamArchiveRaw* stream, int64_t request, int whence)
    {
        (void)a;
        if (stream->m_stream->Seek(request, io::Stream::SeekWhence(whence)) == Status::kOk)
            return stream->m_stream->GetPosition();
        return -1;
    }

    int64_t StreamArchiveRaw::ArchiveSkip(struct archive* a, StreamArchiveRaw* stream, int64_t skip)
    {
        (void)a;
        if (stream->m_stream->Seek(skip, kSeekCurrent) == Status::kFail)
            return 0;
        return skip;
    }
}
// namespace rePlayer