#pragma once

#include <tiostream.h>

#include <Core/String.h>
#include <IO/Stream.h>

namespace rePlayer
{
    using namespace core;

    class TagLibStream : public TagLib::IOStream
    {
    public:
        TagLibStream(io::Stream* stream);
        virtual ~TagLibStream();

        TagLib::FileName name() const;
        TagLib::ByteVector readBlock(size_t length);

        void writeBlock(const TagLib::ByteVector &data);
        void insert(const TagLib::ByteVector &data, TagLib::offset_t start = 0, size_t replace = 0);
        void removeBlock(TagLib::offset_t start = 0, size_t length = 0);
        bool readOnly() const;
        bool isOpen() const;
        void seek(TagLib::offset_t offset, Position p = Beginning);
        void clear();
        TagLib::offset_t tell() const;
        TagLib::offset_t length();
        void truncate(TagLib::offset_t length);

    private:
        core::SmartPtr<io::Stream> m_stream;
    };

    inline TagLibStream::TagLibStream(io::Stream* stream)
        : m_stream(stream)
    {}

    inline TagLibStream::~TagLibStream()
    {}

    inline TagLib::FileName TagLibStream::name() const
    {
        if (m_stream.IsValid())
            return TagLib::FileName(m_stream->GetName().c_str());
        return "";
    }

    inline TagLib::ByteVector TagLibStream::readBlock(size_t length)
    {
        if (m_stream.IsInvalid())
            return {};

        auto size = m_stream->GetSize();
        auto pos = m_stream->GetPosition();

        auto currentLength = pos < size ? size - pos : 0;
        if (currentLength < length)
            length = currentLength;
        if (length == 0)
            return TagLib::ByteVector();

        TagLib::ByteVector buffer(static_cast<unsigned int>(length));
        m_stream->Read(buffer.data(), length);

        return buffer;
    }

    inline void TagLibStream::writeBlock(const TagLib::ByteVector& /*data*/)
    {}

    inline void TagLibStream::insert(const TagLib::ByteVector& /*data*/, TagLib::offset_t /*start*/, size_t /*replace*/)
    {}

    inline void TagLibStream::removeBlock(TagLib::offset_t /*start*/, size_t /*length*/)
    {}

    inline bool TagLibStream::readOnly() const
    {
        return true;
    }

    inline bool TagLibStream::isOpen() const
    {
        return true;
    }

    inline void TagLibStream::seek(TagLib::offset_t offset, Position p)
    {
        if (m_stream.IsValid())
            m_stream->Seek(offset, p == Beginning ? io::Stream::kSeekBegin : p == Current ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd);
    }

    inline void TagLibStream::clear()
    {}

    inline TagLib::offset_t TagLibStream::tell() const
    {
        if (m_stream.IsValid())
            return long(m_stream->GetPosition());
        return 0;
    }

    inline TagLib::offset_t TagLibStream::length()
    {
        if (m_stream.IsValid())
            return m_stream->GetSize();
        return 0;
    }

    inline void TagLibStream::truncate(TagLib::offset_t /*length*/)
    {}
}
// namespace rePlayer