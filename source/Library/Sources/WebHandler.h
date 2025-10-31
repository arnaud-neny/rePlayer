#pragma once

// Core
#include <Core/Log.h>
#include <Core/String.h>

// rePlayer
#include <Replayer/Core.h>

// curl
#include <Curl/curl.h>

// libxml
#include <libxml/HTMLparser.h>

namespace rePlayer
{
    class WebHandler
    {
    public:
        virtual void OnReadNode(xmlNode* node) = 0;

        WebHandler()
            : curl(curl_easy_init())
        {}
        WebHandler(const WebHandler& other)
            : curl(curl_easy_init())
        {
            (void)other;
        }
        WebHandler(WebHandler&& other)
            : curl(other.curl)
            , isInitialized(other.isInitialized)
        {
            other.curl = curl_easy_init();
            other.isInitialized = false;
        }
        WebHandler& operator=(const WebHandler& other)
        {
            (void)other;
            curl_easy_cleanup(curl);
            curl = curl_easy_init();
            isInitialized = false;
            return *this;
        }
        WebHandler& operator=(WebHandler&& other)
        {
            curl_easy_cleanup(curl);
            curl = other.curl;
            isInitialized = other.isInitialized;
            other.curl = curl_easy_init();
            other.isInitialized = false;
            return *this;
        }
        virtual ~WebHandler()
        {
            curl_easy_cleanup(curl);
        }

        template <typename... Arguments>
        Status Fetch(Arguments&&... args)
        {
            error.clear();

            char url[1024];
            core::sprintf(url, std::forward<Arguments>(args)...);

            Array<char> buffer;

            char errorBuffer[CURL_ERROR_SIZE];
            if (!isInitialized)
            {
                isInitialized = true;
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

                curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());
            }
            else
            {
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
            }

            curl_easy_setopt(curl, CURLOPT_URL, url);

            Status status = Status::kOk;
            auto curlErr = curl_easy_perform(curl);
            if (curlErr == CURLE_OK)
            {
                if (auto* doc = htmlReadMemory(buffer.Items(), buffer.NumItems(), nullptr, nullptr, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING))
                {
                    OnReadNode(xmlDocGetRootElement(doc));

                    xmlFreeDoc(doc);
                }
                else
                {
                    error = "libxml: can't parse html";
                    Log::Error("libxml: can't parse html (%s)\n", url);
                    status = Status::kFail;
                }
                xmlResetLastError();
            }
            else
            {
                error = "Curl: ";
                error += curl_easy_strerror(curlErr);
                Log::Error("Curl: %s (%s)\n", curl_easy_strerror(curlErr), url);
                status = Status::kFail;
            }
            return status;
        }

        std::string Escape(const char* buf)
        {
            auto escapeBuf = curl_easy_escape(curl, buf, static_cast<int>(strlen(buf)));
            std::string newBuf = escapeBuf;
            curl_free(escapeBuf);
            return newBuf;
        }

        static size_t WriteCallback(char* in, size_t size, size_t nmemb, Array<char>* out)
        {
            size *= nmemb;
            out->Add(in, uint32_t(size));
            return size;
        }

        static void ConvertString(const xmlChar* text, std::string& output)
        {
            auto* buf = reinterpret_cast<const char*>(text);
            auto size = strlen(buf);
            auto* str = reinterpret_cast<char*>(_alloca(size + 1));
            auto* dst = str;
            auto* spc = str - 1;
            for (uint32_t i = 0; i < size; i++, dst++)
            {
                if (buf[i] == -62 && buf[i + 1] == -96)
                {
                    *dst = ' ';
                    i++;
                }
                else if (buf[i] == '\r' || buf[i] == '\n')
                {
                    break;
                }
                else
                {
                    if (buf[i] != ' ')
                        spc = dst;
                    *dst = buf[i];
                }
            }
            spc[1] = 0;
            output = str;
        }

        CURL* curl;
        bool isInitialized = false;
        std::string error;
    };
}
// namespace rePlayer