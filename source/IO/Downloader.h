#pragma once

// Core
#include <Containers/Array.h>
#include <Core/String.h>

typedef void CURL;
struct curl_slist;

namespace rePlayer
{
    using namespace core;

    class Downloader : public Array<uint8_t>
    {
    public:
        Downloader(const char * urlMessage = nullptr);
        Downloader(const Downloader& other) = delete;
        Downloader(Downloader&& other);

        virtual ~Downloader();

        Downloader& operator=(const Downloader& other) = delete;
        Downloader& operator=(Downloader&& other);

        void AddHTTPHeader(const char* header);
        void SetReferer(const char* referer);

        template <typename... Arguments>
        void SetUrlLog(Arguments&&... args)
        {
            char url[1024];
            sprintf(url, std::forward<Arguments>(args)...);
            m_urlLog = url;
        }

        virtual Status Fetch(const char* url);
        virtual Status OnDownloaded();

        template <typename... Arguments>
        Status Fetch(Arguments... args) requires (sizeof...(Arguments) > 1)
        {
            char url[2048];
            core::sprintf(url, std::forward<Arguments>(args)...);
            return Fetch(url);
        }

        [[nodiscard]] std::string Escape(const char* buf);

        [[nodiscard]] std::string Unescape(const char* buf);

    private:
        static size_t WriteCallback(char* in, size_t size, size_t nmemb, Downloader* out);

    public:
        CURL* m_curl;
        curl_slist* m_headers = nullptr;
        const char* m_urlMessage = nullptr;
        bool m_isInitialized = false;
        std::string m_urlLog;
        std::string m_error;
    };
}
// namespace rePlayer