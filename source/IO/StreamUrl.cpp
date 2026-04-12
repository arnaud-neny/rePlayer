#include "StreamUrl.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <IO/StreamMemory.h>
#include <Thread/Thread.h>

// rePlayer
#include <Replayer/Core.h>

// curl
#include <curl/curl.h>

#include <atomic>

#define LOG_HEADERS 0

namespace rePlayer
{
    SmartPtr<StreamUrl> StreamUrl::Create(const std::string& filename, io::Stream* root)
    {
        auto stream = SmartPtr<StreamUrl>(kAllocate, filename, false, root);
        if (std::atomic_ref(stream->m_state) == State::kFailed)
            stream.Reset();
        else
            stream->AddFilename(filename);
        return stream;
    }

    uint64_t StreamUrl::Read(void* buffer, uint64_t size)
    {
        std::atomic_ref atomicHead(m_head);

        auto* output = reinterpret_cast<uint8_t*>(buffer);
        auto remainingSize = size;
        auto tail = m_tail;
        bool isStarving = false;
        if (m_type == Type::kStreaming)
        {
            while (remainingSize > 0)
            {
                auto head = atomicHead.load();
                auto chunkSize = head - tail;
                // too far behind or connection reset ?
                if (head < tail || chunkSize > kCacheWindow)
                {
                    tail = Max(0, head - kCacheSize + kCacheWindow - size);
                    Log::Warning("StreamUrl: skip (overflow)\n");
                    continue;
                }

                // cancel or something wrong (slow connection)?
                if (chunkSize == 0)
                {
                    if (std::atomic_ref(m_state) >= State::kEnd)
                    {
                        m_tail = head;
                        return size - remainingSize;
                    }
                    // slow connection? to early reads?
                    if (m_isLatencyEnabled)
                    {
                        // wait
                        thread::Sleep(1);
                        if (!isStarving)
                            Log::Warning("StreamUrl: wait (starving)\n");
                    }
                    else
                    {
                        // rewind
                        tail = Max(0, head - kCacheSize + kCacheWindow - size);
                        if (!isStarving)
                            Log::Warning("StreamUrl: skip (starving)\n");
                    }
                    isStarving = true;
                    continue;
                }
                chunkSize = uint32_t(Min(remainingSize, uint64_t(chunkSize)));

                auto dataPos = tail & kCacheMask;
                if (dataPos + chunkSize > kCacheSize)
                {
                    auto splitSize = kCacheSize - dataPos;
                    memcpy(output, m_data.Items(dataPos), splitSize);
                    output += splitSize;
                    tail += splitSize;
                    dataPos = 0;
                    chunkSize -= splitSize;
                    remainingSize -= splitSize;
                }

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

                    thread::Sleep(1);
                    availableSize = atomicHead - tail;
                }

                availableSize = uint32_t(Min(remainingSize, uint64_t(availableSize)));
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
        if (m_type == Type::kStreaming)
        {
            if (whence == SeekWhence::kSeekBegin && offset == 0)
            {
                std::atomic_ref atomicHead(m_head);
                if (m_tail != 0 && atomicHead < kCacheSize / 2)
                {
                    Log::Warning("StreamUrl: Seek reset\n");
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
                    m_infos.Clear();
                    Core::AddJob([this]()
                    {
                        Update();
                    });
                    while (std::atomic_ref(m_state) < State::kDownload)
                        thread::Sleep(1);
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
                    thread::Sleep(1);
                offset += m_head;
            }
            while (offset > int64_t(std::atomic_ref(m_head)) && std::atomic_ref(m_state) < State::kEnd)
                thread::Sleep(1);
            if (offset >= 0 && offset <= int64_t(std::atomic_ref(m_head)))
            {
                m_tail = uint32_t(offset);
                return Status::kOk;
            }
        }
        return Status::kFail;
    }

    uint64_t StreamUrl::GetSize() const
    {
        if (m_type == Type::kStreaming)
            return 0;
        while (std::atomic_ref(m_state) < State::kEnd)
            thread::Sleep(1);
        return m_head;
    }

    int64_t StreamUrl::GetAvailableSize() const
    {
        if (m_type == Type::kStreaming)
            return std::atomic_ref(m_head) - std::atomic_ref(m_tail);
        return std::atomic_ref(m_head);
    }

    uint64_t StreamUrl::GetPosition() const
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
            for (int64_t i = m_infos.NumItems<int64_t>() - 1; i >= 0; --i)
            {
                if (m_infos[i].pos < m_tail)
                {
                    title = m_infos[i].title;
                    break;
                }
            }
        }
        return title;
    }

    std::string StreamUrl::GetArtist() const
    {
        std::string artist;
        if (m_type == Type::kStreaming)
        {
            thread::ScopedSpinLock lock(m_spinLock);
            for (int64_t i = m_infos.NumItems<int64_t>() - 1; i >= 0; --i)
            {
                if (m_infos[i].pos < m_tail)
                {
                    artist = m_infos[i].artist;
                    break;
                }
            }
        }
        return artist;
    }

    const Span<const uint8_t> StreamUrl::Read()
    {
        if (m_type == Type::kStreaming)
            return { nullptr, size_t(0) };

        while (std::atomic_ref(m_state) < State::kEnd)
            thread::Sleep(1);
        return { m_data.Items(), uint32_t(m_head) };
    }

    bool StreamUrl::EnableLatency(bool isEnabled)
    {
        std::swap(m_isLatencyEnabled, isEnabled);
        return isEnabled;
    }

    StreamUrl::StreamUrl(const std::string& url, bool isClone, io::Stream* root)
        : io::Stream(root)
        , m_url(url)
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

        curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 30L); // 30 bytes per sec
        curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, 5L); // 5 seconds check

        Core::AddJob([this]()
        {
            Update();
        });
        while (std::atomic_ref(m_state) < State::kDownload)
            thread::Sleep(1);
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

    SmartPtr<io::Stream> StreamUrl::OnOpen(const std::string& filename)
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
            stream = StreamUrl::Create(filename, GetRoot());
            if (stream.IsInvalid())
                stream = StreamUrl::Create(url, GetRoot());
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
            stream = StreamUrl::Create(url, GetRoot());
            if (stream.IsInvalid())
            {
                stream = StreamUrl::Create(urlEscaped, GetRoot());
                url = std::move(urlEscaped);
            }
        }
        if (stream.IsValid())
            m_links.Add(stream);
        return stream;
    }

    SmartPtr<io::Stream> StreamUrl::OnClone()
    {
        if (m_type == Type::kStreaming)
            return nullptr;

        while (std::atomic_ref(m_state) < State::kEnd)
            thread::Sleep(1);

        auto stream = io::StreamMemory::Create(m_url, m_data.Items(), m_data.Size(), true, this);
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
            thread::Sleep(1);
    }

    void StreamUrl::Update()
    {
        CURLcode curlCode;
        for (;;)
        {
            curlCode = curl_easy_perform(m_curl);
            if (m_type == Type::kStreaming)
            {
                if (curlCode == CURLE_WRITE_ERROR || std::atomic_ref(m_state).load() == State::kCancel)
                    break;
                if (curlCode != CURLE_OK && curlCode != CURLE_RECV_ERROR && curlCode != CURLE_OPERATION_TIMEDOUT && curlCode != CURLE_PARTIAL_FILE)
                    break;
            }
            else
                break;
            Log::Warning("StreamUrl: connection reset \"%s\"\n", curl_easy_strerror(curlCode));
            m_tail = 0;
            m_metadataSize = 0;
            m_chunkSize = 0;
            m_isReadingChunk = true;
            m_infos.Clear();
        }

        if (curlCode == CURLE_OK || curlCode == CURLE_WRITE_ERROR)
        {
            std::atomic_ref(m_state).store(State::kEnd);
        }
        else
        {
            std::atomic_ref(m_state).store(State::kFailed);
            Log::Warning("StreamUrl: %s for \"%s\"\n", curl_easy_strerror(curlCode), m_url.c_str());
        }

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
                auto remainingSize = uint32_t(size);
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
                            auto head = uint32_t(stream->m_head & kCacheMask);
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
                        auto splitSize = uint32_t(kCacheSize - head);
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
                stream->m_data.Add(data, uint32_t(size));
                stream->m_mutex.unlock();
                atomicHead += uint32_t(size);
            }
        }
        return size;
    }

    void StreamUrl::ExtractMetadata()
    {
        std::string title;
        std::string artist;
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

            // Some title are badly encoded (double UTF-8), so revert it
            // Decode one layer of UTF-8 encoding
            auto decode = [](std::string& input)
            {
                auto* str = (unsigned char*)input.data();
                auto oldSize = input.size() + 1; // including nul char
                auto newSize = 0;
                for (size_t i = 0; i < oldSize; i++, newSize++)
                {
                    unsigned char c = str[i];

                    if (c < 0x80)
                    {
                        // ASCII
                        str[newSize] = c;
        }
                    else if ((c >> 5) == 0x6 && str[i + 1])
                    {
                        // 2-byte UTF-8
                        unsigned char c1 = str[i];
                        unsigned char c2 = str[++i];

                        unsigned char decoded =
                            ((c1 & 0x1F) << 6) |
                            (c2 & 0x3F);

                        str[newSize] = decoded;
                    }
//                     else if ((c >> 4) == 0xE)
//                     {
//                         // 3-byte UTF-8 - not reversible to single byte safely
//                         // copy as-is
//                         str[newSize] = c;
//                     }
                    else
                    {
                        str[newSize] = c;
                    }
                }
                input.resize(newSize - 1);
                return oldSize != newSize;
            };
            while (decode(title));

            // most radios do "artist - title"
            auto pos = title.find(" - ");
            if (pos != title.npos)
            {
                artist = title.substr(0, pos);
                title = title.substr(pos + 3);
            }
            else
            {
                // some do "title by artist"
                pos = title.rfind(" by ");
                if (pos != title.npos)
                {
                    artist = title.substr(pos + 4);
                    title = title.substr(0, pos);
                }
            }
        }

        thread::ScopedSpinLock lock(m_spinLock);
        auto minPos = m_head - kCacheSize;
        for (int64_t i = m_infos.NumItems<int64_t>() - 1; i > 0; --i)
        {
            if (m_infos[i].pos < minPos)
            {
                m_infos.RemoveAt(0, uint32_t(i));
                break;
            }
        }
        auto* info = m_infos.Push();
        info->pos = m_head;
        info->title = title;
        info->artist = artist;
    }
}
// namespace rePlayer