#pragma once

#include <IO/Stream.h>

struct archive;
struct archive_entry;

namespace rePlayer
{
    using namespace core;

    class StreamArchive : public core::io::Stream
    {
        friend class SmartPtr<StreamArchive>;
    public:
        static [[nodiscard]] SmartPtr<StreamArchive> Create(const std::string& filename, bool isPackage);
        static [[nodiscard]] SmartPtr<StreamArchive> Create(io::Stream* stream, bool isPackage);

        size_t Read(void* buffer, size_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        [[nodiscard]] size_t GetSize() const final;
        [[nodiscard]] size_t GetPosition() const final;

        [[nodiscard]] const std::string& GetName() const final { return m_entryFilename; }

        [[nodiscard]] std::string GetComments() const final { return m_comments; }

        [[nodiscard]] SmartPtr<Stream> Open(const std::string& filename) final;
        [[nodiscard]] SmartPtr<Stream> Next() final;

        const Span<const uint8_t> Read() final;

    private:
        static constexpr uint32_t kCacheSize = 4096;

    private:
        StreamArchive(io::Stream* stream, bool isPackage);
        ~StreamArchive() final;

        [[nodiscard]] SmartPtr<Stream> OnClone() final;

        void ReOpen();

        static int64_t ArchiveRead(struct archive* a, StreamArchive* stream, const void** buf);
        static int64_t ArchiveSeek(struct archive* a, StreamArchive* stream, int64_t request, int whence);
        static int64_t ArchiveSkip(struct archive* a, StreamArchive* stream, int64_t skip);

    private:
        SmartPtr<io::Stream> m_stream;
        uint8_t m_cache[kCacheSize];

        archive* m_archive;
        archive_entry* m_entry;

        std::string m_comments;
        std::string m_entryFilename;
        bool m_isPackage = false;
        uint32_t m_entryIndex = 0;
        size_t m_entrySize;
        int64_t m_entryPosition = 0;
        uint8_t* m_entryDataBlock;
        size_t m_entryDataBlockSize = 0;
        int64_t m_entryDataBlockPosition = 0;
        SmartPtr<io::Stream> m_streamMemory;
    };
}
// namespace rePlayer