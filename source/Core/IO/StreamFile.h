#pragma once

#include "Stream.h"

namespace core::io
{
    class StreamMemory;
    class StreamFile : public Stream
    {
        friend class SmartPtr<StreamFile>;
    public:
        static SmartPtr<StreamFile> Create(const std::string& filename);

        size_t Read(void* buffer, size_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        size_t GetSize() const final;
        size_t GetPosition() const final;

        const std::string& GetName() const final { return m_name; }

        SmartPtr<Stream> Open(const std::string& filename) override;

        const Span<const uint8_t> Read() final;

    private:
        StreamFile() = default;
        ~StreamFile() final;

        SmartPtr<Stream> OnClone() final;

        static std::wstring Convert(const std::string& name);

    private:
        std::string m_name;
        void* m_handle;
        SmartPtr<StreamMemory> m_stream;
    };
}
// namespace core::io