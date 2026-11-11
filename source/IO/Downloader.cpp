// Core
#include <Core/Log.h>
#include <Core/String.h>

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

            if (m_lowSpeedTime && m_lowSpeedLimit)
            {
                curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, m_lowSpeedLimit);
                curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, m_lowSpeedTime);
            }

            curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
        }
        else
            Clear();

        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

        curl_easy_setopt(m_curl, CURLOPT_URL, url);

        if (m_urlMessage)
            Log::Message(m_urlMessage, url);

        Status status = Status::kOk;
        if (auto curlErr = curl_easy_perform(m_curl); curlErr == CURLE_OK)
        {
            status = OnDownloaded();
        }
        else
        {
            char buf[64];
            auto* err = curl_easy_strerror(curlErr);
            if (long responseCode = 0; curlErr == CURLE_HTTP_RETURNED_ERROR && curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode) == CURLE_OK)
            {
                switch (responseCode)
                {
                // 4xx Client Errors
                case 400:
                    err = "Bad Request";
                    break;
                case 401:
                    err = "Unauthorized";
                    break;
                case 403:
                    err = "Forbidden";
                    break;
                case 404:
                    err = "Not Found";
                    break;
                case 405:
                    err = "Method Not Allowed";
                    break;
                // 5xx Server Errors
                case 500:
                    err = "Internal Server Error";
                    break;
                case 502:
                    err = "Bad Gateway";
                    break;
                case 503:
                    err = "Service Unavailable";
                    break;
                case 504:
                    err = "Gateway Timeout";
                    break;
                default:
                    err = "Unknown HTTP Status";
                }

                sprintf(buf, "%i %s", responseCode, err);
                err = buf;
            }
            m_error = "Curl: ";
            m_error += err;
            Log::Error("Curl: %s (%s)\n", err, m_urlLog.c_str());
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