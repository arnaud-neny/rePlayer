// Core
#include <Core/Log.h>

// rePlayer
#include <Replayer/Core.h>
#include "Downloader.h"

// curl
#include <Curl/curl.h>

namespace rePlayer
{
    Downloader::Downloader(const char * urlMessage)
        : m_curl(curl_easy_init())
        , m_urlMessage(urlMessage)
    {}

    Downloader::Downloader(Downloader&& other)
        : m_curl(other.m_curl)
        , m_isInitialized(other.m_isInitialized)
    {
        other.m_curl = curl_easy_init();
        other.m_isInitialized = false;
    }

    Downloader::~Downloader()
    {
        curl_slist_free_all(m_headers);
        curl_easy_cleanup(m_curl);
    }

    Downloader& Downloader::operator=(Downloader&& other)
    {
        curl_easy_cleanup(m_curl);
        m_curl = other.m_curl;
        m_isInitialized = other.m_isInitialized;
        other.m_curl = curl_easy_init();
        other.m_isInitialized = false;
        return *this;
    }

    void Downloader::AddHTTPHeader(const char* header)
    {
        m_headers = curl_slist_append(m_headers, header);
    }

    void Downloader::SetReferer(const char* referer)
    {
        curl_easy_setopt(m_curl, CURLOPT_REFERER, referer);
    }

    Status Downloader::Fetch(const char* url)
    {
        if (m_urlLog.empty())
            m_urlLog = url;

        if (!m_isInitialized)
        {
            m_isInitialized = true;
            curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, false);
            curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1);
#ifdef _DEBUG
            curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
#endif
            curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1);

            curl_easy_setopt(m_curl, CURLOPT_USERAGENT, Core::GetLabel());

            curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 30L); // 30 bytes per sec
            curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, 5L); // 5 seconds check

            curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
        }
        else
            Clear();

        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

        curl_easy_setopt(m_curl, CURLOPT_URL, url);

        if (m_urlMessage)
            Log::Message(m_urlMessage, url);

        Status status = Status::kOk;
        auto curlErr = curl_easy_perform(m_curl);
        if (curlErr == CURLE_OK)
        {
            status = OnDownloaded();
        }
        else
        {
            m_error = "Curl: ";
            m_error += curl_easy_strerror(curlErr);
            Log::Error("Curl: %s (%s)\n", curl_easy_strerror(curlErr), m_urlLog.c_str());
            status = Status::kFail;
        }
        return status;
    }

    Status Downloader::OnDownloaded()
    {
        return Status::kOk;
    }

    std::string Downloader::Escape(const char* buf)
    {
        auto escapeBuf = curl_easy_escape(m_curl, buf, 0);
        std::string newBuf = escapeBuf;
        curl_free(escapeBuf);
        return newBuf;
    }

    std::string Downloader::Unescape(const char* buf)
    {
        auto unescapeBuf = curl_easy_unescape(m_curl, buf, 0, nullptr);
        std::string newBuf = unescapeBuf;
        curl_free(unescapeBuf);
        return newBuf;
    }

    size_t Downloader::WriteCallback(char* in, size_t size, size_t nmemb, Downloader* out)
    {
        size *= nmemb;
        out->Add(pcCast<uint8_t>(in), uint32_t(size));
        return size;
    }
}
// namespace rePlayer