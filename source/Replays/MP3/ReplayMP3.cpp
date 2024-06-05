#define DR_MP3_IMPLEMENTATION
#include "ReplayMP3.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::MP3,
        .name = "MPEG-3",
        .extensions = "mp2;mp3;aix",
        .about = "dr_mp3 " DRMP3_VERSION_STRING "\nCopyright (c) 2020 David Reid",
        .load = ReplayMP3::Load
    };

    Replay* ReplayMP3::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* mp3 = new drmp3;
        auto* streamData = new StreamData{
            .stream = stream,
            .isSeekable = stream->GetSize() != 0,
            .isInit = true
        };
        if (!drmp3_init(mp3, OnRead, OnSeek, streamData, nullptr))
        {
            delete mp3;
            delete streamData;
            return nullptr;
        }
        bool isValid = mp3->channels <= 2;
        if (isValid && streamData->isSeekable)
            isValid = drmp3_get_pcm_frame_count(mp3) > (mp3->sampleRate / 8);
        if (!isValid)
        {
            drmp3_uninit(mp3);
            delete mp3;
            delete streamData;
            return nullptr;
        }

        streamData->isInit = false;
        return new ReplayMP3(streamData, mp3);
    }

    ReplayMP3::~ReplayMP3()
    {
        drmp3_uninit(m_mp3);
        delete m_mp3;
        delete m_streamData;
    }

    ReplayMP3::ReplayMP3(StreamData* streamData, drmp3* mp3)
        : Replay(mp3->frameInfo.layer == 2 ? eExtension::_mp2 : eExtension::_mp3, eReplay::MP3)
        , m_streamData(streamData)
        , m_mp3(mp3)
    {}

    size_t ReplayMP3::OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead)
    {
        auto* streamData = reinterpret_cast<StreamData*>(pUserData);
        if (streamData->isInit && !streamData->isSeekable && streamData->stream->GetPosition() > 65536)
            return 0;
        return size_t(streamData->stream->Read(pBufferOut, bytesToRead));
    }

    drmp3_bool32 ReplayMP3::OnSeek(void* pUserData, int offset, drmp3_seek_origin origin)
    {
        auto* streamData = reinterpret_cast<StreamData*>(pUserData);
        if (streamData->stream->Seek(offset, origin == drmp3_seek_origin_start ? io::Stream::kSeekBegin : io::Stream::kSeekCurrent) == Status::kOk)
            return DRMP3_TRUE;
        return DRMP3_FALSE;
    }

    bool ReplayMP3::IsSeekable() const
    {
        return m_streamData->isSeekable;
    }

    bool ReplayMP3::IsStreaming() const
    {
        return !m_streamData->isSeekable;
    }

    uint32_t ReplayMP3::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_mp3->channels == 1)
        {
            auto numRenderer = uint32_t(drmp3_read_pcm_frames_f32(m_mp3, numSamples, reinterpret_cast<float*>(output) + numSamples));
            output->ConvertMono(reinterpret_cast<float*>(output) + numSamples, numRenderer);
            return numRenderer;

        }
        return uint32_t(drmp3_read_pcm_frames_f32(m_mp3, numSamples, reinterpret_cast<float*>(output)));
    }

    uint32_t ReplayMP3::Seek(uint32_t timeInMs)
    {
        drmp3_uint64 index = timeInMs;
        index *= GetSampleRate();
        index /= 1000;
        drmp3_seek_to_pcm_frame(m_mp3, index);
        return timeInMs;
    }

    void ReplayMP3::ResetPlayback()
    {
        if (m_streamData->isSeekable)
        {
            drmp3_seek_to_start_of_stream(m_mp3);
        }
        else
        {
            drmp3_uninit(m_mp3);
            m_streamData->isInit = true;
            m_streamData->stream->Seek(0, io::Stream::kSeekBegin);
            drmp3_init(m_mp3, OnRead, OnSeek, m_streamData, nullptr);
            m_streamData->isInit = false;
        }
    }

    void ReplayMP3::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayMP3::SetSubsong(uint32_t)
    {}

    uint32_t ReplayMP3::GetDurationMs() const
    {
        if (m_streamData->isSeekable)
            return static_cast<uint32_t>((1000ull * drmp3_get_pcm_frame_count(m_mp3)) / m_mp3->sampleRate);
        return 0;
    }

    uint32_t ReplayMP3::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayMP3::GetStreamingTitle() const
    {
        std::string title;
        title = m_streamData->stream->GetTitle();
        return title;
    }

    std::string ReplayMP3::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_streamData->stream->GetInfo();
        return metadata;
    }

    std::string ReplayMP3::GetInfo() const
    {
        std::string info;
        info = m_mp3->channels == 1 ? "1 channel\n" : "2 channels\n";
        char buf[128];
        sprintf(buf, "%d kb/s - %d hz\n", m_mp3->frameInfo.bitrate_kbps, m_mp3->frameInfo.hz);
        info += buf;
        info += "dr_mp3 " DRMP3_VERSION_STRING;
        return info;
    }
}
// namespace rePlayer