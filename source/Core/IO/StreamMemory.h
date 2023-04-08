#pragma once

#include "Stream.h"

namespace core::io
{
    class StreamMemory : public Stream
    {
        friend class SmartPtr<StreamMemory>;
        friend class StreamFile;
    public:
        static SmartPtr<StreamMemory> Create(const std::string& filename, const uint8_t* buffer, size_t size, bool isStatic);

        size_t Read(void* buffer, size_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        size_t GetSize() const final;
        size_t GetPosition() const final;

        void SetName(const std::string& filename) { m_name = filename; }
        const std::string& GetName() const final { return m_name; }

        SmartPtr<Stream> Open(const std::string& filename) override;

        const Span<const uint8_t> Read() final;

    private:
        StreamMemory() = default;
        SmartPtr<Stream> OnClone() final;

    private:
        SmartPtr<SharedMemory> m_mem;
        const uint8_t* m_buffer;
        size_t m_size;
        int64_t m_position{ 0 };
        std::string m_name;
    };
}
// namespace core::io