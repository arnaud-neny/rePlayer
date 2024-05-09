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
        virtual uint64_t Read(void* buffer, uint64_t size) = 0;
        virtual Status Seek(int64_t offset, SeekWhence whence) = 0;

        virtual [[nodiscard]] uint64_t GetSize() const = 0;
        virtual [[nodiscard]] uint64_t GetPosition() const = 0;

        virtual void SetName(const std::string& name) { (void)name; }
        virtual [[nodiscard]] const std::string& GetName() const = 0;

        virtual [[nodiscard]] std::string GetComments() const { return {}; }
        virtual [[nodiscard]] std::string GetInfo() const { return {}; }
        virtual [[nodiscard]] std::string GetTitle() const { return {}; }

        [[nodiscard]] SmartPtr<Stream> Clone();

        // open another stream in the same context as the current one
        virtual [[nodiscard]] SmartPtr<Stream> Open(const std::string& filename) = 0;
        // open the next entry in the stream (eg: archive)
        virtual [[nodiscard]] SmartPtr<Stream> Next(bool isForced = false);

        // read all the stream
        virtual [[nodiscard]] const Span<const uint8_t> Read();

    protected:
        Stream() = default;

        virtual [[nodiscard]] SmartPtr<Stream> OnClone() = 0;

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