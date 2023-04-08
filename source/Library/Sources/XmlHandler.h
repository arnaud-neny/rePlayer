#pragma once

#include <Core/Log.h>
#include <Core/String.h>
#include <Curl/curl.h>
#include <Xml/tinyxml2.h>

namespace rePlayer
{
    class XmlHandler
    {
    public:
        using XMLDocument = tinyxml2::XMLDocument;

        virtual void OnReadXml(const XMLDocument& doc) = 0;

        virtual void OnCurlInit(Array<char>& /*xmlBuffer*/) {}

        XmlHandler()
            : curl{ curl_easy_init() }
        {}
        XmlHandler(const XmlHandler& /*other*/)
            : curl{ curl_easy_init() }
        {}
        XmlHandler(XmlHandler&& other)
            : curl{ other.curl }
            , isInitialized{ other.isInitialized }
        {
            other.curl = curl_easy_init();
            other.isInitialized = false;
        }
        XmlHandler& operator=(const XmlHandler& /*other*/)
        {
            curl_easy_cleanup(curl);
            curl = curl_easy_init();
            isInitialized = false;
            return *this;
        }
        XmlHandler& operator=(XmlHandler&& other)
        {
            curl_easy_cleanup(curl);
            curl = other.curl;
            isInitialized = other.isInitialized;
            other.curl = curl_easy_init();
            other.isInitialized = false;
            return *this;
        }
        virtual ~XmlHandler()
        {
            curl_easy_cleanup(curl);
        }

        template <typename... Arguments>
        void Fetch(Arguments&&... args)
        {
            char url[1024];
            sprintf(url, std::forward<Arguments>(args)...);

            char errorBuffer[CURL_ERROR_SIZE];
            Array<char> xmlBuffer;
            if (!isInitialized)
            {
                isInitialized = true;
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xmlBuffer);

                OnCurlInit(xmlBuffer);
            }
            else
            {
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xmlBuffer);
            }

            curl_easy_setopt(curl, CURLOPT_URL, url);

            auto curlErr = curl_easy_perform(curl);
            if (curlErr == CURLE_OK)
            {
                xmlBuffer.Add('\0');
                XMLDocument doc;
                doc.Parse(xmlBuffer.Items());
                if (!doc.Error())
                    OnReadXml(doc);
            }
            else
                Log::Error("Curl: %s (%s)\n", curl_easy_strerror(curlErr), url);
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
            size_t r;
            r = size * nmemb;
            out->Add(in, r);
            return r;
        }

        //helper to get text from a xmlNode reprocessing it to check for entities if it's a CDATA
        static std::string GetText(const tinyxml2::XMLElement* xmlNode)
        {
            auto txtNode = xmlNode->FirstChild()->ToText();
            std::string txt = txtNode->Value();
            if (txtNode->CData())
            {
                tinyxml2::StrPair strPair;
                strPair.Set(txt.data(), txt.data() + txt.size(), tinyxml2::StrPair::NEEDS_ENTITY_PROCESSING);
                txt = strPair.GetStr();
            }
            return txt;
        }

        CURL* curl;
        bool isInitialized = false;
    };
}
// namespace rePlayer