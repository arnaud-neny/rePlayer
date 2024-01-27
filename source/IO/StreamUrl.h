#pragma once

#include <Containers/Array.h>
#include <IO/Stream.h>
#include <Thread/SpinLock.h>

#include <mutex>

typedef struct Curl_easy CURL;
struct curl_slist;

namespace rePlayer
{
    using namespace core;

    class StreamUrl : public core::io::Stream
    {
        friend class SmartPtr<StreamUrl>;
    public:
        static SmartPtr<StreamUrl> Create(const std::string& url);

        size_t Read(void* buffer, size_t size) final;
        Status Seek(int64_t offset, SeekWhence whence) final;

        size_t GetSize() const final;
        size_t GetPosition() const final;

        const std::string& GetName() const final { return m_url; }

        std::string GetInfo() const final;
        std::string GetTitle() const final;

        SmartPtr<Stream> Open(const std::string& filename) override;

        const Span<const uint8_t> Read() final;

    private:
        static constexpr uint64_t kCacheSize = 512 * 1024;
        static constexpr uint64_t kCacheMask = kCacheSize - 1;

    private:
        StreamUrl(const std::string& filename, bool isClone = false);
        ~StreamUrl() override;

        SmartPtr<Stream> OnClone() final;

        void Close();

        static uint32_t ThreadFunc(uint32_t* lpdwParam);
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

        void* m_threadHandle = nullptr;

        uint64_t m_tail = 0;
        uint64_t m_head = 0;
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

        Array<uint8_t> m_data;
        std::mutex m_mutex;
        mutable thread::SpinLock m_spinLock;
    };
}
// namespace rePlayer