// Core
// #include <Core/Log.h>
#include <Core/String.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>

// rePlayer
// #include <Replayer/Core.h>
#include "StreamArchive.h"

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

namespace rePlayer
{
    SmartPtr<StreamArchive> StreamArchive::Create(const std::string& filename, bool isPackage)
    {
        auto stream = io::StreamFile::Create(filename);
        if (stream.IsValid())
            return Create(stream, isPackage);
        return nullptr;
    }

    SmartPtr<StreamArchive> StreamArchive::Create(io::Stream* stream, bool isPackage)
    {
        if (stream)
        {
            SmartPtr<StreamArchive> archiveStream(kAllocate, stream, isPackage);
            if (archiveStream->m_stream)
            {
                if (archive_read_next_header(archiveStream->m_archive, &archiveStream->m_entry) == ARCHIVE_OK)
                {
                    archiveStream->m_entryFilename = archive_entry_pathname(archiveStream->m_entry);
                    archiveStream->m_entrySize = uint32_t(archive_entry_size(archiveStream->m_entry));
                    return archiveStream;
                }
            }
        }
        return nullptr;
    }

    uint64_t StreamArchive::Read(void* buffer, uint64_t size)
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
            memcpy(dest, m_entryDataBlock + m_entryDataBlockPosition, size_t(sizeToCopy));
            dest += sizeToCopy;
            m_entryPosition += sizeToCopy;
            m_entryDataBlockPosition += sizeToCopy;
            sizeLeft -= sizeToCopy;
        }

        return size - sizeLeft;
    }

    Status StreamArchive::Seek(int64_t offset, SeekWhence whence)
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

    uint64_t StreamArchive::GetSize() const
    {
        return m_entrySize;
    }

    uint64_t StreamArchive::GetPosition() const
    {
        if (m_streamMemory.IsValid())
            return m_streamMemory->GetPosition();
        return m_entryPosition;
    }

    SmartPtr<io::Stream> StreamArchive::Open(const std::string& filename)
    {
        SmartPtr<StreamArchive> stream(kAllocate, m_stream->Clone(), m_isPackage);
        if (stream->m_stream.IsValid())
        {
            while (archive_read_next_header(stream->m_archive, &stream->m_entry) == ARCHIVE_OK)
            {
                auto* entryName = archive_entry_pathname(stream->m_entry);
                if (_stricmp(entryName, filename.c_str()) == 0)
                {
                    stream->m_entryFilename = entryName;
                    stream->m_entrySize = uint32_t(archive_entry_size(stream->m_entry));
                    return stream;
                }
                stream->m_entryIndex++;
            }
        }
        return nullptr;
    }

    SmartPtr<io::Stream> StreamArchive::Next(bool isForced)
    {
        if (isForced || !m_isPackage)
        {
            SmartPtr<StreamArchive> stream(kAllocate, m_stream->Clone(), m_isPackage);
            if (stream->m_stream.IsValid())
            {
                while (archive_read_next_header(stream->m_archive, &stream->m_entry) == ARCHIVE_OK)
                {
                    if (stream->m_entryIndex > m_entryIndex)
                    {
                        stream->m_entryFilename = archive_entry_pathname(stream->m_entry);
                        stream->m_entrySize = uint32_t(archive_entry_size(stream->m_entry));
                        return stream;
                    }
                    stream->m_entryIndex++;
                }
            }
        }
        return nullptr;
    }

    const Span<const uint8_t> StreamArchive::Read()
    {
        if (m_streamMemory.IsValid())
            return m_streamMemory->Read();

        auto data = Stream::Read();
        m_streamMemory = io::StreamMemory::Create(m_entryFilename, m_cachedData, data.Size());
        return data;
    }

    StreamArchive::StreamArchive(io::Stream* stream, bool isPackage)
        : m_stream(stream)
        , m_archive(archive_read_new())
        , m_isPackage(isPackage)
    {
        stream->Seek(0, kSeekBegin);

        archive_read_support_filter_all(m_archive);
        archive_read_support_format_all(m_archive);

        archive_read_set_read_callback(m_archive, reinterpret_cast<archive_read_callback*>(ArchiveRead));
        archive_read_set_seek_callback(m_archive, reinterpret_cast<archive_seek_callback*>(ArchiveSeek));
        archive_read_set_skip_callback(m_archive, reinterpret_cast<archive_skip_callback*>(ArchiveSkip));
        archive_read_set_callback_data(m_archive, this);
        if (archive_read_open1(m_archive) == ARCHIVE_OK)
        {
            // maybe we added a magic header at the end of the file
            auto position = stream->GetPosition();
            stream->Seek(-8, kSeekEnd);
            char buf[8];
            stream->Read(buf, 8);
            stream->Seek(position, kSeekBegin);
            for (uint32_t i = 0; i < 8; i++)
            {
                if (!((buf[i] >= 'A' && buf[i] <= 'Z')
                    || buf[i] == '-'
                    || (buf[i] >= '0' && buf[i] <= '9')))
                    return;
            }
            m_comments.assign(buf, buf + 8);
        }
        else
            m_stream.Reset();
    }

    StreamArchive::~StreamArchive()
    {
        archive_read_free(m_archive);
    }

    SmartPtr<io::Stream> StreamArchive::OnClone()
    {
        SmartPtr<StreamArchive> stream(kAllocate, m_stream->Clone(), m_isPackage);
        if (stream->m_stream.IsValid())
        {
            if (m_streamMemory.IsValid())
                stream->m_streamMemory = m_streamMemory->Clone();
            while (archive_read_next_header(stream->m_archive, &stream->m_entry) == ARCHIVE_OK)
            {
                if (stream->m_entryIndex == m_entryIndex)
                {
                    stream->m_entryFilename = m_entryFilename;
                    stream->m_entrySize = m_entrySize;
                    return stream;
                }
                stream->m_entryIndex++;
            }
            return stream;
        }
        return nullptr;
    }

    void StreamArchive::ReOpen()
    {
        archive_read_free(m_archive);

        m_archive = archive_read_new();

        archive_read_support_filter_all(m_archive);
        archive_read_support_format_all(m_archive);

        archive_read_set_read_callback(m_archive, reinterpret_cast<archive_read_callback*>(ArchiveRead));
        archive_read_set_seek_callback(m_archive, reinterpret_cast<archive_seek_callback*>(ArchiveSeek));
        archive_read_set_skip_callback(m_archive, reinterpret_cast<archive_skip_callback*>(ArchiveSkip));
        archive_read_set_callback_data(m_archive, this);

        m_entryPosition = 0;
        m_entryDataBlockSize = 0;
        m_entryDataBlockPosition = 0;

        m_stream->Seek(0, kSeekBegin);
        archive_read_open1(m_archive);
        for (uint32_t i = 0; i <= m_entryIndex; i++)
            archive_read_next_header(m_archive, &m_entry);
    }

    int64_t StreamArchive::ArchiveRead(struct archive* a, StreamArchive* stream, const void** buf)
    {
        (void)a;
        *buf = stream->m_cache;
        return int64_t(stream->m_stream->Read(stream->m_cache, kCacheSize));
    }

    int64_t StreamArchive::ArchiveSeek(struct archive* a, StreamArchive* stream, int64_t request, int whence)
    {
        (void)a;
        if (stream->m_stream->Seek(request, io::Stream::SeekWhence(whence)) == Status::kOk)
            return stream->m_stream->GetPosition();
        return -1;
    }

    int64_t StreamArchive::ArchiveSkip(struct archive* a, StreamArchive* stream, int64_t skip)
    {
        (void)a;
        if (stream->m_stream->Seek(skip, kSeekCurrent) == Status::kFail)
            return 0;
        return skip;
    }
}
// namespace rePlayer