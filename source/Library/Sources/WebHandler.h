#pragma once

// rePlayer
#include <IO/Downloader.h>

// libxml
#include <libxml/HTMLparser.h>

namespace rePlayer
{
    class WebHandler : public Downloader
    {
    public:
        virtual void OnReadNode(xmlNode* node) = 0;

        WebHandler(const char* encoding = nullptr)
            : encoding(encoding)
        {}
        WebHandler(WebHandler&& other)
            : Downloader(std::move(other))
            , encoding(other.encoding)
        {}
        WebHandler& operator=(WebHandler&& other)
        {
            Downloader::operator=(std::move(other));
            encoding = other.encoding;
            return *this;
        }

        Status OnDownloaded() override
        {
            auto status = Status::kOk;
            if (auto* doc = htmlReadMemory(Items<char>(), NumItems(), nullptr, encoding, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING))
            {
                OnReadNode(xmlDocGetRootElement(doc));

                xmlFreeDoc(doc);
            }
            else
            {
                m_error = "libxml: can't parse html";
                Log::Error("libxml: can't parse html (%s)\n", m_urlLog.c_str());
                status = Status::kFail;
            }
            xmlResetLastError();
            return status;
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

        const char* encoding;
    };
}
// namespace rePlayer