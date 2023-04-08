#define DR_FLAC_IMPLEMENTATION
#include "ReplayFLAC.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::FLAC,
        .name = "Free Lossless Audio Codec",
        .extensions = "flac",
        .about = "dr_flac " DRFLAC_VERSION_STRING "\nCopyright (c) 2020 David Reid",
        .load = ReplayFLAC::Load
    };

    Replay* ReplayFLAC::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto replay = new ReplayFLAC(stream);
        replay->m_flac = drflac_open_with_metadata(OnRead, OnSeek, OnMetadata, replay, nullptr);
        if (!replay->m_flac || replay->m_flac->channels != 2)
        {
            delete replay;
            return nullptr;
        }

        return replay;
    }

    ReplayFLAC::~ReplayFLAC()
    {
        drflac_close(m_flac);
    }

    ReplayFLAC::ReplayFLAC(io::Stream* stream)
        : Replay(eExtension::_flac, eReplay::FLAC)
        , m_stream(stream)
    {}

    size_t ReplayFLAC::OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead)
    {
        auto stream = reinterpret_cast<ReplayFLAC*>(pUserData)->m_stream;
        return stream->Read(pBufferOut, bytesToRead);
    }

    drflac_bool32 ReplayFLAC::OnSeek(void* pUserData, int offset, drflac_seek_origin origin)
    {
        auto stream = reinterpret_cast<ReplayFLAC*>(pUserData)->m_stream;
        if (stream->Seek(offset, origin == drflac_seek_origin_start ? io::Stream::kSeekBegin : io::Stream::kSeekCurrent) == Status::kOk)
            return DRFLAC_TRUE;
        return DRFLAC_FALSE;
    }

    void ReplayFLAC::OnMetadata(void* pUserData, drflac_metadata* pMetadata)
    {
        auto This = reinterpret_cast<ReplayFLAC*>(pUserData);
        if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
        {
            drflac_vorbis_comment_iterator it;
            drflac_init_vorbis_comment_iterator(&it, pMetadata->data.vorbis_comment.commentCount, pMetadata->data.vorbis_comment.pComments);
            drflac_uint32 length;
            while (auto comment = drflac_next_vorbis_comment(&it, &length))
            {
                if (!This->m_info.empty())
                    This->m_info += "\n";
                This->m_info.append(comment, length);
            }
        }
    }

    uint32_t ReplayFLAC::Render(StereoSample* output, uint32_t numSamples)
    {
        return uint32_t(drflac_read_pcm_frames_f32(m_flac, numSamples, reinterpret_cast<float*>(output)));
    }

    uint32_t ReplayFLAC::Seek(uint32_t timeInMs)
    {
        drflac_uint64 index = timeInMs;
        index *= GetSampleRate();
        index /= 1000;
        drflac_seek_to_pcm_frame(m_flac, index);
        return timeInMs;
    }

    void ReplayFLAC::ResetPlayback()
    {
        drflac_seek_to_pcm_frame(m_flac, 0);
    }

    void ReplayFLAC::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayFLAC::SetSubsong(uint16_t)
    {}

    uint32_t ReplayFLAC::GetDurationMs() const
    {
        return static_cast<uint32_t>((1000ull * m_flac->totalPCMFrameCount) / m_flac->sampleRate);
    }

    uint32_t ReplayFLAC::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayFLAC::GetExtraInfo() const
    {
        return m_info;
    }

    std::string ReplayFLAC::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        char buf[128];
        sprintf(buf, "%dbits - %dHz\n", m_flac->bitsPerSample, m_flac->sampleRate);
        info += buf;
        info += "dr_flac " DRFLAC_VERSION_STRING;
        return info;
    }
}
// namespace rePlayer