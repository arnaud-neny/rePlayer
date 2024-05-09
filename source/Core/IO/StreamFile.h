#pragma once

#include "Stream.h"

namespace core::io
{
    class StreamMemory;
    class StreamFile : public Stream
    {
        friend class SmartPtr<StreamFile>;
    public:
        static [[nodiscard]] SmartPtr<StreamFile> Create(const std::string& filename);
        static [[nodiscard]] SmartPtr<StreamFile> Create(const std::wstring& filename);

        uint64_t Read(void* buffer, uint64_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        [[nodiscard]] uint64_t GetSize() const final;
        [[nodiscard]] uint64_t GetPosition() const final;

        [[nodiscard]] const std::string& GetName() const final { return m_name; }

        [[nodiscard]] SmartPtr<Stream> Open(const std::string& filename) override;

        [[nodiscard]] const Span<const uint8_t> Read() final;

    private:
        StreamFile() = default;
        ~StreamFile() final;

        [[nodiscard]] SmartPtr<Stream> OnClone() final;

        static [[nodiscard]] std::wstring Convert(const std::string& name);
        static [[nodiscard]] std::string Convert(const std::wstring& wName);

    private:
        std::string m_name;
        void* m_handle;
        SmartPtr<StreamMemory> m_stream;
    };
}
// namespace core::io