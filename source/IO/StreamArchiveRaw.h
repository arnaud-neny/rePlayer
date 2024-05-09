#pragma once

#include <IO/Stream.h>

struct archive;
struct archive_entry;

namespace rePlayer
{
    using namespace core;

    class StreamArchiveRaw : public core::io::Stream
    {
        friend class SmartPtr<StreamArchiveRaw>;
    public:
        static [[nodiscard]] SmartPtr<StreamArchiveRaw> Create(io::Stream* stream);

        uint64_t Read(void* buffer, uint64_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        [[nodiscard]] uint64_t GetSize() const final;
        [[nodiscard]] uint64_t GetPosition() const final;

        [[nodiscard]] const std::string& GetName() const final;

        [[nodiscard]] SmartPtr<Stream> Open(const std::string& filename) final;

        const Span<const uint8_t> Read() final;

    private:
        static constexpr uint32_t kCacheSize = 4096;

    private:
        StreamArchiveRaw(io::Stream* stream);
        ~StreamArchiveRaw() final;

        [[nodiscard]] SmartPtr<Stream> OnClone() final;

        void ReOpen();

        static int64_t ArchiveRead(struct archive* a, StreamArchiveRaw* stream, const void** buf);
        static int64_t ArchiveSeek(struct archive* a, StreamArchiveRaw* stream, int64_t request, int whence);
        static int64_t ArchiveSkip(struct archive* a, StreamArchiveRaw* stream, int64_t skip);

    private:
        SmartPtr<io::Stream> m_stream;
        uint8_t m_cache[kCacheSize];

        archive* m_archive;
        archive_entry* m_entry;

        std::string m_entryFilename;
        uint64_t m_entrySize = 0;
        int64_t m_entryPosition = 0;
        uint8_t* m_entryDataBlock;
        size_t m_entryDataBlockSize = 0;
        int64_t m_entryDataBlockPosition = 0;
        SmartPtr<io::Stream> m_streamMemory;
    };
}
// namespace rePlayer