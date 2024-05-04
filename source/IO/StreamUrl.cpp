#include "StreamUrl.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>

// rePlayer
#include <Replayer/Core.h>

// curl
#include <curl/curl.h>

#include <atomic>

#define LOG_HEADERS 0

namespace rePlayer
{
    SmartPtr<StreamUrl> StreamUrl::Create(const std::string& filename)
    {
        auto stream = SmartPtr<StreamUrl>(kAllocate, filename);
        if (std::atomic_ref(stream->m_state) == State::kFailed)
            stream.Reset();
        return stream;
    }

    size_t StreamUrl::Read(void* buffer, size_t size)
    {
        std::atomic_ref atomicHead(m_head);

        auto* output = reinterpret_cast<uint8_t*>(buffer);
        auto remainingSize = size;
        auto tail = m_tail;
        if (m_type == Type::kStreaming)
        {
            while (remainingSize > 0)
            {
                auto chunkSize = atomicHead - tail;
                if (chunkSize >= kCacheSize)
                {
                    tail += chunkSize & ~kCacheMask;
                    chunkSize &= kCacheMask;
                }
                // in a perfect world, we shouldn't wait
                while (chunkSize == 0)
                {
                    ::Sleep(1);
                    chunkSize = atomicHead - tail;
                    if (std::atomic_ref(m_state) >= State::kEnd)
                    {
                        m_tail = atomicHead;
                        return size - remainingSize;
                    }
                }
                chunkSize = Min(remainingSize, chunkSize);

                auto dataPos = tail & kCacheMask;
                if (dataPos + chunkSize > kCacheSize)
                {
                    auto splitSize = kCacheSize - dataPos;
                    assert(splitSize <= kCacheSize);
                    memcpy(output, m_data.Items(dataPos), splitSize);
                    output += splitSize;
                    tail += splitSize;
                    dataPos = 0;
                    chunkSize -= splitSize;
                    remainingSize -= splitSize;
                }

                assert(chunkSize <= kCacheSize);
                memcpy(output, m_data.Items(dataPos), chunkSize);
                output += chunkSize;
                tail += chunkSize;
                remainingSize -= chunkSize;
            }
        }
        else
        {
            while (remainingSize > 0)
            {
                auto availableSize = atomicHead - tail;
                for (; availableSize == 0;)
                {
                    if (std::atomic_ref(m_state) >= State::kEnd && atomicHead == tail)
                    {
                        m_tail = tail;
                        return size - remainingSize;
                    }

                    ::Sleep(1);
                    availableSize = atomicHead - tail;
                }

                availableSize = Min(remainingSize, availableSize);
                m_mutex.lock();
                memcpy(output, m_data.Items(tail), availableSize);
                m_mutex.unlock();
                output += availableSize;
                tail += availableSize;
                remainingSize -= availableSize;
            }
        }
        m_tail = tail;
        return size;
    }

    Status StreamUrl::Seek(int64_t offset, SeekWhence whence)
    {
        if (std::atomic_ref(m_state) == State::kFailed)
            return Status::kFail;
        if (m_type != Type::kDownload)
        {
            if (whence == SeekWhence::kSeekBegin && offset == 0)
            {
                std::atomic_ref atomicHead(m_head);
                if (m_tail != 0 && atomicHead < kCacheSize / 2)
                {
                    m_tail = 0;
                }
                else
                {
                    Close();
                    m_state = State::kStart;
                    m_type = Type::kUnknown;
                    m_tail = 0;
                    m_head = 0;
                    m_icySize = 0;
                    m_metadataSize = 0;
                    m_chunkSize = 0;
                    m_isReadingChunk = true;
                    m_isJobDone = false;
                    Core::AddJob([this]()
                    {
                        Update();
                    });
                    while (std::atomic_ref(m_state) < State::kDownload)
                        ::Sleep(1);
                }
                return Status::kOk;
            }
        }
        else
        {
            if (whence == SeekWhence::kSeekCurrent)
                offset += m_tail;
            else if (whence == SeekWhence::kSeekEnd)
            {
                while (std::atomic_ref(m_state) < State::kEnd)
                    ::Sleep(1);
                offset += m_head;
            }
            while (offset > int64_t(std::atomic_ref(m_head)) && std::atomic_ref(m_state) < State::kEnd)
                ::Sleep(1);
            if (offset >= 0 && offset <= int64_t(std::atomic_ref(m_head)))
            {
                m_tail = offset;
                return Status::kOk;
            }
        }
        return Status::kFail;
    }

    size_t StreamUrl::GetSize() const
    {
        if (m_type == Type::kStreaming)
            return 0;
        while (std::atomic_ref(m_state) < State::kEnd)
            ::Sleep(1);
        return m_head;
    }

    size_t StreamUrl::GetPosition() const
    {
        return m_tail;
    }

    std::string StreamUrl::GetInfo() const
    {
        std::string info;
        if (m_type == Type::kStreaming)
        {
            if (!m_icyName.empty())
            {
                info += "Name: ";
                info += m_icyName;
            }
            if (!m_icyDescription.empty())
            {
                if (!info.empty())
                    info += "\n";
                info += "Description: ";
                info += m_icyDescription;
            }
            if (!m_icyGenre.empty())
            {
                if (!info.empty())
                    info += "\n";
                info += "Genre: ";
                info += m_icyGenre;
            }
            if (!m_icyUrl.empty())
            {
                if (!info.empty())
                    info += "\n";
                info += "URL: ";
                info += m_icyUrl;
            }
        }
        return info;
    }

    std::string StreamUrl::GetTitle() const
    {
        std::string title;
        if (m_type == Type::kStreaming)
        {
            thread::ScopedSpinLock lock(m_spinLock);
            title = m_title;
        }
        return title;
    }

    SmartPtr<io::Stream> StreamUrl::Open(const std::string& filename)
    {
        if (filename == m_url)
            return Clone();

        SmartPtr<StreamUrl> stream;
        std::string url;
        if (filename.find("://") != std::string::npos)
        {
            // go to the uncommon part
            size_t startPos = 0;
            while (startPos < filename.size() && startPos < m_url.size() && filename[startPos] == m_url[startPos])
                startPos++;
            // make it curl friendly
            url = Escape(filename, startPos);
            // already fetch?
            for (auto& link : m_links)
            {
                if (link->m_url == filename || link->m_url == url)
                    return link->Clone();
            }
            // download the default, and if it fails, then do the escaped
            stream = StreamUrl::Create(filename);
            if (stream.IsInvalid())
                stream = StreamUrl::Create(url);
            else
                url = filename;
        }
        else
        {
            // behave like a path, so try to remove the original filename
            url = m_url;
            auto pos = url.rfind('/');
            url.resize(pos + 1);
            auto urlEscaped = url;
            // append the filename
            url += filename;
            // append the curl friendly filename
            urlEscaped += Escape(filename, 0);
            // already fetch?
            for (auto& link : m_links)
            {
                if (link->m_url == url || link->m_url == urlEscaped)
                    return link->Clone();
            }
            // download the default, and if it fails, then do the escaped
            stream = StreamUrl::Create(url);
            if (stream.IsInvalid())
            {
                stream = StreamUrl::Create(urlEscaped);
                url = std::move(urlEscaped);
            }
        }
        if (stream.IsValid())
            m_links.Add(stream);
        return stream;
    }

    const Span<const uint8_t> StreamUrl::Read()
    {
        if (m_type == Type::kStreaming)
            return { nullptr, size_t(0) };

        while (std::atomic_ref(m_state) < State::kEnd)
            ::Sleep(1);
        return { m_data.Items(), size_t(m_head) };
    }

    StreamUrl::StreamUrl(const std::string& url, bool isClone)
        : m_url(url)
        , m_isJobDone(isClone)
    {
        if (isClone)
            return;

        // stream is validated from the headers (with response code 200):
        // "icy-name" is valid:  Icecast protocol for streaming: https://cast.readme.io/ and https://gist.github.com/ePirat/adc3b8ba00d85b7e3870
        // "content-length" is valid: download file
        // otherwise it's a download with unknown size

        m_curl = curl_easy_init();

        curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, false);

        m_httpHeaders = curl_slist_append(m_httpHeaders, "Icy-MetaData: 1");
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_httpHeaders);

        m_http200Aliases = curl_slist_append(m_http200Aliases, "ICY");
        curl_easy_setopt(m_curl, CURLOPT_HTTP200ALIASES, m_http200Aliases);

        curl_easy_setopt(m_curl, CURLOPT_REFERER, url.c_str());
        char buf[128];
        sprintf(buf, "rePlayer %u.%u.%u", Core::GetVersion() >> 28, (Core::GetVersion() >> 14) & ((1 << 14) - 1), Core::GetVersion() & ((1 << 14) - 1));
        curl_easy_setopt(m_curl, CURLOPT_USERAGENT, buf);

        curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, OnCurlHeader);
        curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, this);

        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, OnCurlWrite);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);

        Core::AddJob([this]()
        {
            Update();
        });
        while (std::atomic_ref(m_state) < State::kDownload)
            ::Sleep(1);
    }

    StreamUrl::~StreamUrl()
    {
        Close();

        if (m_curl)
        {
            curl_slist_free_all(m_http200Aliases);
            curl_slist_free_all(m_httpHeaders);

            curl_easy_cleanup(m_curl);
        }
    }

    SmartPtr<io::Stream> StreamUrl::OnClone()
    {
        if (m_type == Type::kStreaming)
            return nullptr;

        while (std::atomic_ref(m_state) < State::kEnd)
            ::Sleep(1);

        SmartPtr<StreamUrl> stream(kAllocate, m_url, true);
        stream->m_url = m_url;
        stream->m_icyName = m_icyName;
        stream->m_icyDescription = m_icyDescription;
        stream->m_icyGenre = m_icyGenre;
        stream->m_icyUrl = m_icyUrl;
        stream->m_metadata = m_metadata;
        stream->m_head = m_head;
        stream->m_state = m_state;
        stream->m_type = m_type;
        stream->m_data = m_data;
        return stream;
    }

    std::string StreamUrl::Escape(const std::string& url, size_t startPos)
    {
        auto pos = url.find_first_not_of('/', startPos);
        if (pos == std::string::npos)
            return url;
        std::string newUrl = url.substr(0, pos);
        do
        {
            auto next = url.find_first_of('/', pos);
            if (next == std::string::npos)
            {
                auto* e = curl_easy_escape(m_curl, url.c_str() + pos, int(url.size() - pos));
                newUrl += e;
                curl_free(e);
                break;
            }
            else
            {
                auto* e = curl_easy_escape(m_curl, url.c_str() + pos, int(next - pos));
                newUrl += e;
                newUrl += '/';
                curl_free(e);
                pos = next + 1;
            }
        } while (1);
        return newUrl;
    }

    void StreamUrl::Close()
    {
        std::atomic_ref(this->m_state).store(State::kCancel);
        while (!std::atomic_ref(m_isJobDone).load())
            ::Sleep(1);
    }

    void StreamUrl::Update()
    {
        CURLcode curlCode;
        curlCode = curl_easy_perform(m_curl);
        assert(curlCode == CURLE_OK || curlCode == CURLE_WRITE_ERROR || curlCode == CURLE_HTTP_RETURNED_ERROR || curlCode == CURLE_OPERATION_TIMEDOUT || curlCode == CURLE_COULDNT_CONNECT || curlCode == CURLE_COULDNT_RESOLVE_HOST || curlCode == CURLE_URL_MALFORMAT);

        if (curlCode == CURLE_OK || curlCode == CURLE_WRITE_ERROR)
            std::atomic_ref(m_state).store(State::kEnd);
        else
            std::atomic_ref(m_state).store(State::kFailed);

        std::atomic_ref(m_isJobDone).store(true);
    }

    size_t StreamUrl::OnCurlHeader(const char* buffer, size_t size, size_t count, StreamUrl* stream)
    {
        size *= count;
        if (size)
        {
#if LOG_HEADERS
            Log::Message("StreamUrl: %s", buffer);
#endif

            std::string lowerUrl = ToLower(buffer);

            auto getValue = [&]<int StrLen>(const char(&label)[StrLen], std::string& value)
            {
                if (auto* s = strstr(lowerUrl.c_str(), label))
                {
                    auto pos = lowerUrl.find_first_of(':', s + StrLen - 1 - lowerUrl.c_str());
                    if (pos != lowerUrl.npos)
                    {
                        while (lowerUrl[++pos] == ' ');
                        auto end = lowerUrl.size();
                        while (lowerUrl[end - 1] == '\r' || lowerUrl[end - 1] == '\n' || lowerUrl[end - 1] == ' ')
                            --end;
                        value = std::string(buffer + pos, buffer + end);
                        return true;
                    }
                }
                return false;
            };

            if (stream->m_type != Type::kStreaming)
            {
                uint32_t downloadSize = 0;
                if (getValue("icy-name", stream->m_icyName))
                {
                    stream->m_type = Type::kStreaming;
                    stream->m_data.Resize(kCacheSize);
                    return size;
                }
                else if (_snscanf_s(lowerUrl.c_str(), size, "content-length:%u", &downloadSize) == 1)
                {
                    stream->m_type = Type::kDownload;
                    stream->m_data.Clear();
                    stream->m_data.Reserve(downloadSize);
                    return size;
                }
            }
            if (_snscanf_s(lowerUrl.c_str(), size, "icy-metaint:%u", &stream->m_icySize) == 1
                || getValue("icy-description", stream->m_icyDescription)
                || getValue("icy-genre", stream->m_icyGenre)
                || getValue("icy-url", stream->m_icyUrl))
                return size;
        }
        return size;
    }

    size_t StreamUrl::OnCurlWrite(const uint8_t* data, size_t size, size_t count, StreamUrl* stream)
    {
        auto state = std::atomic_ref(stream->m_state).load();
        if (state == State::kCancel)
            return CURL_WRITEFUNC_ERROR;
        else if (state != State::kDownload)
            std::atomic_ref(stream->m_state) = State::kDownload;

        size *= count;
        if (size)
        {
            std::atomic_ref atomicHead(stream->m_head);
            if (stream->m_type == Type::kStreaming)
            {
                auto remainingSize = size;
                if (stream->m_icySize > 0)
                {
                    while (remainingSize > 0)
                    {
                        if (stream->m_isReadingChunk)
                        {
                            auto chunkSize = stream->m_chunkSize;
                            if ((chunkSize + remainingSize) >= stream->m_icySize)
                            {
                                chunkSize = stream->m_icySize - chunkSize;

                                stream->m_chunkSize = 0;
                                stream->m_isReadingChunk = false;
                            }
                            else
                            {
                                stream->m_chunkSize = uint32_t(chunkSize + remainingSize);
                                chunkSize = uint32_t(remainingSize);
                            }

                            // copy only the last data fitting the cache
                            remainingSize -= (chunkSize - 1) & ~kCacheMask;
                            data += (chunkSize - 1) & ~kCacheMask;
                            chunkSize = ((chunkSize - 1) & kCacheMask) + 1;

                            // split the copy on ring buffer overflow
                            auto head = stream->m_head & kCacheMask;
                            if (head + chunkSize > kCacheSize)
                            {
                                auto splitSize = kCacheSize - head;
                                assert((splitSize <= kCacheSize) && ((head + splitSize) <= kCacheSize));
                                memcpy(stream->m_data.Items(head), data, splitSize);
                                chunkSize -= uint32_t(splitSize);
                                head = 0;
                                data += splitSize;
                                remainingSize -= splitSize;
                                atomicHead += splitSize;
                            }

                            // copy the buffer
                            assert((chunkSize <= kCacheSize) && ((head + chunkSize) <= kCacheSize));
                            memcpy(stream->m_data.Items(head), data, chunkSize);
                            data += chunkSize;
                            remainingSize -= chunkSize;
                            atomicHead += chunkSize;
                        }
                        else if (stream->m_metadataSize == 0)
                        {
                            stream->m_metadataSize = data[0] * 16;
                            stream->m_isReadingChunk = stream->m_metadataSize == 0;
                            remainingSize--;
                            data++;
                            stream->m_metadata.clear();
                        }
                        else
                        {
                            auto skipSize = Min(stream->m_metadataSize, remainingSize);
                            stream->m_metadata.append(reinterpret_cast<const char*>(data), skipSize);
                            data += skipSize;
                            remainingSize -= skipSize;
                            stream->m_metadataSize -= uint32_t(skipSize);
                            stream->m_isReadingChunk = stream->m_metadataSize == 0;
                            if (stream->m_isReadingChunk)
                                stream->ExtractMetadata();
                        }
                    }
                }
                else
                {
                    // copy only the last data fitting the cache
                    data += (remainingSize - 1) & ~kCacheMask;
                    remainingSize = ((remainingSize - 1) & kCacheMask) + 1;

                    // split the copy on ring buffer overflow
                    auto head = stream->m_head & kCacheMask;
                    if (head + remainingSize > kCacheSize)
                    {
                        auto splitSize = kCacheSize - head;
                        assert((splitSize <= kCacheSize) && ((head + splitSize) <= kCacheSize));
                        memcpy(stream->m_data.Items(head), data, splitSize);
                        head = 0;
                        data += splitSize;
                        remainingSize -= splitSize;
                        atomicHead += splitSize;
                    }

                    // copy the buffer
                    assert((remainingSize <= kCacheSize) && ((head + remainingSize) <= kCacheSize));
                    memcpy(stream->m_data.Items(head), data, remainingSize);
                    atomicHead += remainingSize;
                }
            }
            else // basic download
            {
                stream->m_mutex.lock();
                stream->m_data.Add(data, size);
                stream->m_mutex.unlock();
                atomicHead += size;
            }
        }
        return size;
    }

    void StreamUrl::ExtractMetadata()
    {
        std::string title;
        auto first = m_metadata.find("StreamTitle='");
        if (first != m_metadata.npos)
        {
            first += sizeof("StreamTitle='") - 1;
            auto last = m_metadata.find("\';", first);
            if (last != m_metadata.npos)
                title.append(m_metadata.c_str() + first, m_metadata.c_str() + last);
            else
            {
                last = m_metadata.rfind('\'');
                if (last != m_metadata.npos)
                    title.append(m_metadata.c_str() + first, m_metadata.c_str() + last);
            }
        }

        thread::ScopedSpinLock lock(m_spinLock);
        m_title = title;
    }
}
// namespace rePlayer