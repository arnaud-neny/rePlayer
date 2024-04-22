#pragma once

#include "Stream.h"

namespace core::io
{
    class StreamMemory : public Stream
    {
        friend class SmartPtr<StreamMemory>;
    public:
        static [[nodiscard]] SmartPtr<StreamMemory> Create(const std::string& filename, const uint8_t* buffer, size_t size, bool isStatic);
        static [[nodiscard]] SmartPtr<StreamMemory> Create(const std::string& filename, SharedMemory* sharedMemory, size_t size);

        size_t Read(void* buffer, size_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        [[nodiscard]] size_t GetSize() const final;
        [[nodiscard]] size_t GetPosition() const final;

        void SetName(const std::string& filename) final { m_name = filename; }
        [[nodiscard]] const std::string& GetName() const final { return m_name; }

        [[nodiscard]] SmartPtr<Stream> Open(const std::string& filename) override;

        [[nodiscard]] const Span<const uint8_t> Read() final;

    private:
        StreamMemory() = default;
        [[nodiscard]] SmartPtr<Stream> OnClone() final;

    private:
        SmartPtr<SharedMemory> m_mem;
        const uint8_t* m_buffer;
        size_t m_size;
        int64_t m_position{ 0 };
        std::string m_name;
    };
}
// namespace core::io