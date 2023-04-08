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
        return stream->Read(ptr, size * nmemb);
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

    uint32_t ReplayVorbis::Render(StereoSample* output, uint32_t numSamples)
    {
        float** pcm;
        auto numReadSamples = ov_read_float(m_vorbis, &pcm, numSamples, nullptr);
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
        if (ov_seekable(m_vorbis))
        {
            ogg_int64_t index = timeInMs;
            index *= GetSampleRate();
            index /= 1000;
            ov_pcm_seek(m_vorbis, index);
            return timeInMs;
        }
        return Replay::Seek(timeInMs);
    }

    void ReplayVorbis::ResetPlayback()
    {
        ov_raw_seek(m_vorbis, 0);
    }

    void ReplayVorbis::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayVorbis::SetSubsong(uint16_t)
    {}

    uint32_t ReplayVorbis::GetDurationMs() const
    {
        return static_cast<uint32_t>(ov_time_total(m_vorbis, -1) * 1000ull);
    }

    uint32_t ReplayVorbis::GetNumSubsongs() const
    {
        return 1;
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
        return info;
    }

    std::string ReplayVorbis::GetInfo() const
    {
        std::string info;
        auto vi = ov_info(m_vorbis, -1);
        info = vi->channels == 1 ? "1 channel\n" : "2 channels\n";
        char buf[64];
        sprintf(buf, "%dkb/s - %dhz\n", (vi->bitrate_upper > 0 && vi->bitrate_lower > 0) ? (vi->bitrate_upper + vi->bitrate_lower) / 2000 : (vi->bitrate_nominal / 1000), vi->rate);
        info += buf;
        info += "minivorbis 1.3.4/1.3.7";
        return info;
    }
}
// namespace rePlayer