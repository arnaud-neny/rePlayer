#pragma once

#include <Containers/Array.h>
#include <IO/Stream.h>
#include <Thread/SpinLock.h>

#include <mutex>

typedef void CURL;
struct curl_slist;

namespace rePlayer
{
    using namespace core;

    class StreamUrl : public io::Stream
    {
        friend class SmartPtr<StreamUrl>;
    public:
        static SmartPtr<StreamUrl> Create(const std::string& url, io::Stream* root = nullptr);

        uint64_t Read(void* buffer, uint64_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        uint64_t GetSize() const final;
        uint64_t GetPosition() const final;

        const std::string& GetName() const final { return m_url; }

        std::string GetInfo() const final;
        std::string GetTitle() const final;

        const Span<const uint8_t> Read() final;

    private:
        static constexpr uint32_t kCacheSize = 512 * 1024;
        static constexpr uint32_t kCacheMask = kCacheSize - 1;

    private:
        StreamUrl(const std::string& filename, bool isClone, io::Stream* root);
        ~StreamUrl() override;

        SmartPtr<Stream> OnOpen(const std::string& filename) final;
        SmartPtr<Stream> OnClone() final;

        std::string Escape(const std::string& url, size_t startPos);

        void Close();

        void Update();
        static size_t OnCurlHeader(const char* buffer, size_t size, size_t count, StreamUrl* radio);
        static size_t OnCurlWrite(const uint8_t* data, size_t size, size_t count, StreamUrl* radio);
        void ExtractMetadata();

    private:
        CURL* m_curl = nullptr;
        curl_slist* m_httpHeaders = nullptr;
        curl_slist* m_http200Aliases = nullptr;

        std::string m_url;

        std::string m_icyName;
        std::string m_icyDescription;
        std::string m_icyGenre;
        std::string m_icyUrl;
        std::string m_metadata;

        std::string m_title;

        uint32_t m_tail = 0;
        uint32_t m_head = 0;
        uint32_t m_icySize = 0;
        uint32_t m_metadataSize = 0;
        uint32_t m_chunkSize = 0;
        bool m_isReadingChunk = true;

        enum class State : uint8_t
        {
            kStart,
            kDownload,
            kCancel,
            kEnd,
            kFailed
        } m_state = State::kStart;
        enum class Type : uint8_t
        {
            kUnknown,
            kDownload,
            kStreaming
        } m_type = Type::kUnknown;
        bool m_isJobDone;

        Array<uint8_t> m_data;
        std::mutex m_mutex;
        mutable thread::SpinLock m_spinLock;

        Array<SmartPtr<StreamUrl>> m_links;
    };
}
// namespace rePlayer