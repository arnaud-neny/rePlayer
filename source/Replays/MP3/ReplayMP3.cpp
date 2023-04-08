#define DR_MP3_IMPLEMENTATION
#include "ReplayMP3.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::MP3,
        .name = "MPEG-3",
        .extensions = "mp3",
        .about = "dr_mp3 " DRMP3_VERSION_STRING "\nCopyright (c) 2020 David Reid",
        .load = ReplayMP3::Load
    };

    Replay* ReplayMP3::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* mp3 = new drmp3;
        if (!drmp3_init(mp3, OnRead, OnSeek, stream, nullptr))
        {
            delete mp3;
            return nullptr;
        }
        if (mp3->channels != 2)
        {
            drmp3_uninit(mp3);
            delete mp3;
            return nullptr;
        }

        return new ReplayMP3(stream, mp3);
    }

    ReplayMP3::~ReplayMP3()
    {
        drmp3_uninit(m_mp3);
        delete m_mp3;
    }

    ReplayMP3::ReplayMP3(io::Stream* stream, drmp3* mp3)
        : Replay(eExtension::_mp3, eReplay::MP3)
        , m_stream(stream)
        , m_mp3(mp3)
    {}

    size_t ReplayMP3::OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead)
    {
        auto stream = reinterpret_cast<io::Stream*>(pUserData);
        return stream->Read(pBufferOut, bytesToRead);
    }

    drmp3_bool32 ReplayMP3::OnSeek(void* pUserData, int offset, drmp3_seek_origin origin)
    {
        auto stream = reinterpret_cast<io::Stream*>(pUserData);
        if (stream->Seek(offset, origin == drmp3_seek_origin_start ? io::Stream::kSeekBegin : io::Stream::kSeekCurrent) == Status::kOk)
            return DRMP3_TRUE;
        return DRMP3_FALSE;
    }

    uint32_t ReplayMP3::Render(StereoSample* output, uint32_t numSamples)
    {
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
        drmp3_seek_to_start_of_stream(m_mp3);
    }

    void ReplayMP3::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayMP3::SetSubsong(uint16_t)
    {}

    uint32_t ReplayMP3::GetDurationMs() const
    {
        auto numFrames = drmp3_get_pcm_frame_count(m_mp3);
        return static_cast<uint32_t>((1000ull * numFrames) / m_mp3->sampleRate);
    }

    uint32_t ReplayMP3::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayMP3::GetExtraInfo() const
    {
        return {};
    }

    std::string ReplayMP3::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        char buf[128];
        sprintf(buf, "%d kb/s - %dHz\n", m_mp3->frameInfo.bitrate_kbps, m_mp3->frameInfo.hz);
        info += buf;
        info += "dr_mp3 " DRMP3_VERSION_STRING;
        return info;
    }
}
// namespace rePlayer