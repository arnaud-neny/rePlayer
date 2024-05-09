#include "Stream.h"

#include <Core.h>

namespace core::io
{
    SmartPtr<Stream> Stream::Clone()
    {
        auto stream = OnClone();
        if (stream.IsValid())
            stream->m_cachedData = m_cachedData;
        return stream;
    }

    Stream::SharedMemory::~SharedMemory()
    {
        Free(m_ptr);
    }

    const Span<const uint8_t> Stream::Read()
    {
        auto size = GetSize();
        if (m_cachedData.IsInvalid())
        {
            m_cachedData.New(Alloc(size_t(size)));
            Seek(0, SeekWhence::kSeekBegin);
            Read(m_cachedData->m_ptr, size);
        }
        return { reinterpret_cast<const uint8_t*>(m_cachedData->m_ptr), uint32_t(size) };
    }

    SmartPtr<Stream> Stream::Next(bool isForced)
    {
        (void)isForced;
        return nullptr;
    }
}
// namespace core::io