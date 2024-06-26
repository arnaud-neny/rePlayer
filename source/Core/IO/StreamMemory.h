#pragma once

#include "Stream.h"

namespace core::io
{
    class StreamMemory : public Stream
    {
        friend class SmartPtr<StreamMemory>;
    public:
        static [[nodiscard]] SmartPtr<StreamMemory> Create(const std::string& filename, const uint8_t* buffer, uint64_t size, bool isStatic, Stream* root = nullptr);
        static [[nodiscard]] SmartPtr<StreamMemory> Create(const std::string& filename, SharedMemory* sharedMemory, uint64_t size, Stream* root = nullptr);

        uint64_t Read(void* buffer, uint64_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        [[nodiscard]] uint64_t GetSize() const final;
        [[nodiscard]] uint64_t GetPosition() const final;

        void SetName(const std::string& filename) final { m_name = filename; }
        [[nodiscard]] const std::string& GetName() const final { return m_name; }

        [[nodiscard]] const Span<const uint8_t> Read() final;

    private:
        StreamMemory(Stream* root);

        [[nodiscard]] SmartPtr<Stream> OnOpen(const std::string& filename) final;
        [[nodiscard]] SmartPtr<Stream> OnClone() final;

    private:
        SmartPtr<SharedMemory> m_mem;
        const uint8_t* m_buffer;
        uint64_t m_size;
        int64_t m_position = 0;
        std::string m_name;
    };
}
// namespace core::io