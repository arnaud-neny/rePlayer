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
        TagLib::ByteVector readBlock(unsigned long length);

        void writeBlock(const TagLib::ByteVector &data);
        void insert(const TagLib::ByteVector &data, unsigned long start = 0, unsigned long replace = 0);
        void removeBlock(unsigned long start = 0, unsigned long length = 0);
        bool readOnly() const;
        bool isOpen() const;
        void seek(long offset, Position p = Beginning);
        void clear();
        long tell() const;
        long length();
        void truncate(long length);

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
        return TagLib::FileName(m_stream->GetName().c_str());
    }

    inline TagLib::ByteVector TagLibStream::readBlock(unsigned long length)
    {
        auto size = m_stream->GetSize();
        auto pos = m_stream->GetPosition();

        auto currentLength = pos < size ? size - pos : 0;
        if (currentLength < length)
            length = (unsigned long)currentLength;
        if (length == 0)
            return TagLib::ByteVector();

        TagLib::ByteVector buffer(static_cast<unsigned int>(length));
        m_stream->Read(buffer.data(), length);

        return buffer;
    }

    inline void TagLibStream::writeBlock(const TagLib::ByteVector& /*data*/)
    {}

    inline void TagLibStream::insert(const TagLib::ByteVector& /*data*/, unsigned long /*start*/, unsigned long /*replace*/)
    {}

    inline void TagLibStream::removeBlock(unsigned long /*start*/, unsigned long /*length*/)
    {}

    inline bool TagLibStream::readOnly() const
    {
        return true;
    }

    inline bool TagLibStream::isOpen() const
    {
        return true;
    }

    inline void TagLibStream::seek(long offset, Position p)
    {
        m_stream->Seek(offset, p == Beginning ? io::Stream::kSeekBegin : p == Current ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd);
    }

    inline void TagLibStream::clear()
    {}

    inline long TagLibStream::tell() const
    {
        return long(m_stream->GetPosition());
    }

    inline long TagLibStream::length()
    {
        return long(m_stream->GetSize());
    }

    inline void TagLibStream::truncate(long /*length*/)
    {}
}
// namespace rePlayer