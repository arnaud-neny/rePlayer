#include "ReplayVorbis.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

extern "C"
{
    void* myAlloc(size_t s) { return core::Alloc(s, 16); }
    void* myCalloc(size_t c, size_t s) { return core::Calloc(c, s); }
    void* myRealloc(void* p, size_t s) { return core::Realloc(p, s, 16); }
    void myFree(void* p) { core::Free(p); }
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Vorbis,
        .name = "Vorbis",
        .extensions = "ogg",
        .about = "minivorbis 1.3.4/1.3.7\nCopyright (c) 2002-2020 Xiph.org Foundation\nCopyright (c) 2020 Eduardo Bart",
        .load = ReplayVorbis::Load
    };

    Replay* ReplayVorbis::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* vorbis = new OggVorbis_File;;
        if (ov_open_callbacks(stream, vorbis, nullptr, 0, { OnRead, OnSeek, nullptr, OnTell }) || vorbis->vi->channels > 2)
        {
            delete vorbis;
            return nullptr;
        }
        return new ReplayVorbis(stream, vorbis);
    }

    ReplayVorbis::~ReplayVorbis()
    {
        ov_clear(m_vorbis);
        delete m_vorbis;
    }

    ReplayVorbis::ReplayVorbis(io::Stream* stream, OggVorbis_File* vorbis)
        : Replay(eExtension::_ogg, eReplay::Vorbis)
        , m_stream(stream)
        , m_vorbis(vorbis)
    {}

    size_t ReplayVorbis::OnRead(void* ptr, size_t size, size_t nmemb, void* datasource)
    {
        auto stream = reinterpret_cast<io::Stream*>(datasource);
        return size_t(stream->Read(ptr, size * nmemb));
    }

    int32_t ReplayVorbis::OnSeek(void* datasource, ogg_int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(datasource);
        if (stream->Seek(offset, whence == SEEK_SET ? io::Stream::kSeekBegin : whence == SEEK_CUR ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd) == Status::kOk)
            return 0;
        return -1;
    }

    long ReplayVorbis::OnTell(void* datasource)
    {
        auto stream = reinterpret_cast<io::Stream*>(datasource);
        return long(stream->GetPosition());
    }

    bool ReplayVorbis::IsSeekable() const
    {
        return ov_seekable(m_vorbis);
    }

    bool ReplayVorbis::IsStreaming() const
    {
        return !ov_seekable(m_vorbis);
    }

    uint32_t ReplayVorbis::Render(StereoSample* output, uint32_t numSamples)
    {
        float** pcm;
        int64_t numReadSamples;
        do
        {
            numReadSamples = ov_read_float(m_vorbis, &pcm, numSamples, nullptr);
        } while (numReadSamples == OV_HOLE);
        if (numReadSamples <= 0)
            return 0;
        numSamples = uint32_t(numReadSamples);
        if (m_vorbis->vi->channels == 2)
        {
            for (uint32_t i = 0; i < numSamples; i++)
            {
                output->left = *pcm[0]++;
                output->right = *pcm[1]++;
                output++;
            }
        }
        else
        {
            for (uint32_t i = 0; i < numSamples; i++)
            {
                output->left = output->right = *pcm[0]++;
                output++;
            }
        }
        return numSamples;
    }

    uint32_t ReplayVorbis::Seek(uint32_t timeInMs)
    {
        ogg_int64_t index = timeInMs;
        index *= GetSampleRate();
        index /= 1000;
        ov_pcm_seek(m_vorbis, index);
        return timeInMs;
    }

    void ReplayVorbis::ResetPlayback()
    {
        if (ov_seekable(m_vorbis))
            ov_raw_seek(m_vorbis, 0);
        m_stream->Seek(0, io::Stream::kSeekBegin);
        ov_clear(m_vorbis);
        ov_open_callbacks(m_stream.Get(), m_vorbis, nullptr, 0, { OnRead, OnSeek, nullptr, OnTell });
    }

    void ReplayVorbis::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayVorbis::SetSubsong(uint32_t)
    {}

    uint32_t ReplayVorbis::GetDurationMs() const
    {
        auto timeTotal = ov_time_total(m_vorbis, -1);
        if (timeTotal != OV_EINVAL)
            return static_cast<uint32_t>(timeTotal * 1000ull);
        return 0;
    }

    uint32_t ReplayVorbis::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayVorbis::GetStreamingTitle() const
    {
        std::string title;
        if (IsStreaming())
        {
            auto comments = ov_comment(m_vorbis, -1);
            int index = 0;
            for (auto* comment = vorbis_comment_query(comments, "title", index); comment; comment = vorbis_comment_query(comments, "title", ++index))
            {
                if (index)
                    title += '-';
                title += comment;
            }
        }
        return title;
    }

    std::string ReplayVorbis::GetStreamingArtist() const
    {
        std::string artist;
        if (IsStreaming())
        {
            auto comments = ov_comment(m_vorbis, -1);
            int index = 0;
            for (auto* comment = vorbis_comment_query(comments, "artist", index); comment; comment = vorbis_comment_query(comments, "artist", ++index))
            {
                if (index)
                    artist += " & ";
                artist += comment;
            }
        }
        return artist;
    }

    std::string ReplayVorbis::GetExtraInfo() const
    {
        std::string info;
        auto comments = ov_comment(m_vorbis, -1);
        for (int32_t i = 0; i < comments->comments; i++)
        {
            if (!info.empty())
                info += "\n";
            info.append(comments->user_comments[i], comments->comment_lengths[i]);
        }
        auto streamInfo = m_stream->GetInfo();
        if (!streamInfo.empty())
        {
            if (!info.empty())
                info += "\n";
            info += streamInfo;
        }
        return info;
    }

    std::string ReplayVorbis::GetInfo() const
    {
        std::string info;
        auto vi = ov_info(m_vorbis, -1);
        info = vi->channels == 1 ? "1 channel\n" : "2 channels\n";
        char buf[64];
        sprintf(buf, "%d kb/s - %d hz\n", (vi->bitrate_upper > 0 && vi->bitrate_lower > 0) ? (vi->bitrate_upper + vi->bitrate_lower + 1999) / 2000 : ((vi->bitrate_nominal + 999) / 1000), vi->rate);
        info += buf;
        info += "minivorbis 1.3.4/1.3.7";
        return info;
    }
}
// namespace rePlayer