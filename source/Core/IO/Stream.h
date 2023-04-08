#pragma once

#include <Containers/SmartPtr.h>
#include <Containers/Span.h>
#include <Core/RefCounted.h>

#include <string>

namespace core::io
{
    class Stream : public RefCounted
    {
    public:
        enum SeekWhence
        {
            kSeekBegin,
            kSeekCurrent,
            kSeekEnd
        };

    public:
        virtual size_t Read(void* buffer, size_t size) = 0;
        virtual Status Seek(int64_t offset, SeekWhence whence) = 0;

        virtual size_t GetSize() const = 0;
        virtual size_t GetPosition() const = 0;

        virtual const std::string& GetName() const = 0;

        SmartPtr<Stream> Clone();

        // open another stream in the same context as the current one
        virtual SmartPtr<Stream> Open(const std::string& filename) = 0;

        // read all the stream
        virtual const Span<const uint8_t> Read();

    protected:
        Stream() = default;

        virtual SmartPtr<Stream> OnClone() = 0;

    private:
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;

    protected:
        struct SharedMemory : public RefCounted
        {
            SharedMemory(void* ptr) : m_ptr(ptr) {}
            ~SharedMemory();
            void* m_ptr;
        };
        SmartPtr<SharedMemory> m_cachedData;
    };
}
// namespace core::io