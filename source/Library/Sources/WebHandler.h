#pragma once

#include <Core/Log.h>
#include <Core/String.h>
#include <Curl/curl.h>
#include <Tidy/tidy.h>
#include <Tidy/tidybuffio.h>

#include <Replayer/Core.h>

#include <stdint.h>
#include <string>

namespace rePlayer
{
    class WebHandler
    {
    public:
        virtual void OnReadNode(TidyDoc doc, TidyNode tnod) = 0;

        virtual void OnCurlInit(TidyBuffer& docbuf) {}

        WebHandler()
            : curl{ curl_easy_init() }
        {}
        WebHandler(const WebHandler& other)
            : curl{ curl_easy_init() }
        {}
        WebHandler(WebHandler&& other)
            : curl{ other.curl }
            , isInitialized{ other.isInitialized }
        {
            other.curl = curl_easy_init();
            other.isInitialized = false;
        }
        WebHandler& operator=(const WebHandler& other)
        {
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
        void Fetch(Arguments&&... args)
        {
            char url[1024];
            sprintf(url, std::forward<Arguments>(args)...);

            TidyBuffer docbuf = { 0 };
            TidyBuffer tidy_errbuf = { 0 };

            tidySetMallocCall([](size_t len) { return Alloc(len); });
            tidySetReallocCall([](void* buf, size_t len) { return Realloc(buf, len); });
            tidySetFreeCall([](void* buf) { Free(buf); });
            auto tdoc = tidyCreate();
            tidyOptSetBool(tdoc, TidyForceOutput, yes);
            tidyOptSetInt(tdoc, TidyOutCharEncoding, TidyEncUtf8);
            tidyOptSetBool(tdoc, TidyQuoteAmpersand, no);
            tidyOptSetBool(tdoc, TidyQuoteMarks, no);
            tidyOptSetBool(tdoc, TidyQuoteNbsp, yes);
            tidyOptSetBool(tdoc, TidyPreserveEntities, no);
            tidyOptSetInt(tdoc, TidyWrapLen, 0);
            tidyBufInit(&docbuf);

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
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &docbuf);

                curl_easy_setopt(curl, CURLOPT_USERAGENT, Core::GetLabel());

                OnCurlInit(docbuf);
            }
            else
            {
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &docbuf);
            }

            curl_easy_setopt(curl, CURLOPT_URL, url);

            auto curlErr = curl_easy_perform(curl);
            if (curlErr == CURLE_OK)
            {
                tidySetErrorBuffer(tdoc, &tidy_errbuf);

                auto tidyErr = tidyParseBuffer(tdoc, &docbuf);
                if (tidyErr >= 0)
                {
                    tidyErr = tidyCleanAndRepair(tdoc);
                    if (tidyErr >= 0)
                    {
                        tidyErr = tidyRunDiagnostics(tdoc);
                        if (tidyErr >= 0)
                        {
                            OnReadNode(tdoc, tidyGetRoot(tdoc));
                        }
                    }
                }

                if (tidy_errbuf.allocator)
                    tidyBufFree(&tidy_errbuf);
            }
            else
                Log::Error("Curl: %s (%s)\n", curl_easy_strerror(curlErr), url);

            tidyBufFree(&docbuf);
            tidyRelease(tdoc);
        }

        std::string Escape(const char* buf)
        {
            auto escapeBuf = curl_easy_escape(curl, buf, static_cast<int>(strlen(buf)));
            std::string newBuf = escapeBuf;
            curl_free(escapeBuf);
            return newBuf;
        }

        static size_t WriteCallback(char* in, size_t size, size_t nmemb, TidyBuffer* out)
        {
            size_t r;
            r = size * nmemb;
            tidyBufAppend(out, in, static_cast<uint32_t>(r));
            return r;
        }

        static void ConvertString(const char* buf, uint32_t size, std::string& output)
        {
            auto str = reinterpret_cast<char*>(_alloca(size + 1));
            auto dst = str;
            for (uint32_t i = 0; i < size; i++, dst++)
            {
                if (buf[i] == -96)
                {
                    *dst = ' ';
                }
                else if (buf[i] == '\r' || buf[i] == '\n')
                {
                    *dst = 0;
                    break;
                }
                else if (buf[i] == '&')
                {
                    i += ConvertEntity(buf + i, dst);
                }
                else
                {
                    *dst = buf[i];
                }
            }
            str[size] = 0;
            output = str;
        }

        template <typename Predicate>
        static void ReadNodeText(TidyDoc doc, TidyNode tnod, Predicate&& predicate)
        {
            TidyBuffer buf;
            tidyBufInit(&buf);
            tidyNodeGetText(doc, tnod, &buf);
            predicate(reinterpret_cast<char*>(buf.bp), buf.size);
            tidyBufFree(&buf);
        }

        CURL* curl;
        bool isInitialized = false;
    };
}
// namespace rePlayer