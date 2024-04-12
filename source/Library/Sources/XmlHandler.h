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
    class XmlHandler
    {
    public:
        virtual void OnReadNode(xmlNode* node) = 0;

        XmlHandler()
            : curl(curl_easy_init())
        {}
        XmlHandler(const XmlHandler& other)
            : curl(curl_easy_init())
        {
            (void)other;
        }
        XmlHandler(XmlHandler&& other)
            : curl(other.curl)
            , isInitialized(other.isInitialized)
        {
            other.curl = curl_easy_init();
            other.isInitialized = false;
        }
        XmlHandler& operator=(const XmlHandler& other)
        {
            (void)other;
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

            auto curlErr = curl_easy_perform(curl);
            if (curlErr == CURLE_OK)
            {
                if (auto* doc = xmlReadMemory(buffer.Items(), buffer.NumItems(), nullptr, nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING))
                {
                    OnReadNode(xmlDocGetRootElement(doc));

                    xmlFreeDoc(doc);
                }
                else
                    Log::Error("libxml: can't parse xml (%s)\n", url);
                xmlResetLastError();
            }
            else
                Log::Error("Curl: %s (%s)\n", curl_easy_strerror(curlErr), url);
        }

        static xmlNode* FindNode(xmlNode* node, const char* name)
        {
            if (node == nullptr)
                return nullptr;
            if (node->type == XML_ELEMENT_NODE)
            {
                if (xmlStrcmp(node->name, BAD_CAST name) == 0)
                    return node;
            }
            if (auto* foundNode = FindNode(node->children, name))
                return foundNode;
            return FindNode(node->next, name);
        }

        static xmlNode* FindChildNode(xmlNode* node, const char* name)
        {
            for (node = node->children; node; node = node->next)
            {
                if (node->type == XML_ELEMENT_NODE)
                {
                    if (xmlStrcmp(node->name, BAD_CAST name) == 0)
                        return node;
                }
            }
            return nullptr;
        }

        static xmlNode* NextNode(xmlNode* node, const char* name)
        {
            for (node = node->next; node; node = node->next)
            {
                if (node->type == XML_ELEMENT_NODE)
                {
                    if (xmlStrcmp(node->name, BAD_CAST name) == 0)
                        return node;
                }
            }
            return nullptr;
        }

        static uint32_t UnsignedText(xmlNode* node, uint32_t defaultValue = 0)
        {
            uint32_t value = defaultValue;
            for (node = node->children; node; node = node->next)
            {
                if (node->type == XML_TEXT_NODE)
                {
                    if (sscanf_s((const char*)node->content, "%u", &value) == 1)
                        return value;
                }
            }
            return value;
        }

        static std::string Text(xmlNode* node)
        {
            for (node = node->children; node; node = node->next)
            {
                if (node->type == XML_TEXT_NODE)
                    return (const char*)node->content;
                else if (node->type == XML_CDATA_SECTION_NODE)
                {
                    std::string value;
                    for (auto* c = (const char*)node->content; *c; c++)
                    {
                        // decode entities
                        if (c[0] == '&')
                        {
                            int code = 0;
                            if (c[1] == '#')
                            {
                                if (c[2] == 'x' || c[2] == 'X')
                                    sscanf_s(c, "&#%x;", &code);
                                else
                                    sscanf_s(c, "&#%u;", &code);
                            }
                            else
                            {
                                for (auto* e = c + 1;; e++)
                                {
                                    if (*e == 0)
                                        break;
                                    if (*e == ';')
                                    {
                                        *((char*)e) = '\0';
                                        if (auto* desc = htmlEntityLookup(BAD_CAST c + 1))
                                            code = desc->value;
                                        *((char*)e) = ';';
                                        break;
                                    }
                                }
                            }
                            if (code)
                            {
                                int bits;
                                if (code < 0x80)
                                {
                                    value += char(code);
                                    bits = -6;
                                }
                                else if (code < 0x800)
                                {
                                    value += char(((code >> 6) & 0x1F) | 0xC0);
                                    bits = 0;
                                }
                                else if (code < 0x10000)
                                {
                                    value += char(((code >> 12) & 0x0F) | 0xE0);
                                    bits = 6;
                                }
                                else
                                {
                                    value += char(((code >> 18) & 0x07) | 0xF0);
                                    bits = 12;
                                }
                                for (; bits >= 0; bits -= 6)
                                {
                                    value += char(((code >> bits) & 0x3F) | 0x80);
                                }
                                for (;;)
                                {
                                    if (c[1] == 0)
                                        break;
                                    if (c[0] == ';')
                                        break;
                                    c++;
                                }
                            }
                            else
                                value += c[0];
                        }
                        else
                            value += c[0];
                    }
                    return value;
                }
            }
            return {};
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
/*
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
*/

        CURL* curl;
        bool isInitialized = false;
    };
}
// namespace rePlayer