#define DR_WAV_IMPLEMENTATION
#include "ReplayWAV.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::WAV,
        .name = "Waveform Audio (dr_wav)",
        .extensions = "wav",
        .about = "dr_wav " DRWAV_VERSION_STRING "\nCopyright (c) 2023 David Reid",
        .load = ReplayWAV::Load
    };

    Replay* ReplayWAV::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* wav = new drwav;
        if (!drwav_init_with_metadata(wav, OnRead, OnSeek, stream, 0, nullptr))
        {
            delete wav;
            return nullptr;
        }
        if (wav->channels != 2)
        {
            drwav_uninit(wav);
            delete wav;
            return nullptr;
        }

        return new ReplayWAV(stream, wav);
    }

    ReplayWAV::~ReplayWAV()
    {
        drwav_uninit(m_wav);
        delete m_wav;
    }

    ReplayWAV::ReplayWAV(io::Stream* stream, drwav* wav)
        : Replay(eExtension::_wav, eReplay::WAV)
        , m_stream(stream)
        , m_wav(wav)
    {}

    size_t ReplayWAV::OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead)
    {
        auto stream = reinterpret_cast<io::Stream*>(pUserData);
        return stream->Read(pBufferOut, bytesToRead);
    }

    drwav_bool32 ReplayWAV::OnSeek(void* pUserData, int offset, drwav_seek_origin origin)
    {
        auto stream = reinterpret_cast<io::Stream*>(pUserData);
        if (stream->Seek(offset, origin == drwav_seek_origin_start ? io::Stream::kSeekBegin : io::Stream::kSeekCurrent) == Status::kOk)
            return DRWAV_TRUE;
        return DRWAV_FALSE;
    }

    uint32_t ReplayWAV::Render(StereoSample* output, uint32_t numSamples)
    {
        return uint32_t(drwav_read_pcm_frames_f32(m_wav, numSamples, reinterpret_cast<float*>(output)));
    }

    uint32_t ReplayWAV::Seek(uint32_t timeInMs)
    {
        drwav_uint64 index = timeInMs;
        index *= GetSampleRate();
        index /= 1000;
        drwav_seek_to_pcm_frame(m_wav, index);
        return timeInMs;
    }

    void ReplayWAV::ResetPlayback()
    {
        drwav_seek_to_pcm_frame(m_wav, 0);
    }

    void ReplayWAV::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayWAV::SetSubsong(uint16_t)
    {}

    uint32_t ReplayWAV::GetDurationMs() const
    {
        drwav_uint64 numFrames;
        drwav_get_length_in_pcm_frames(m_wav, &numFrames);
        return static_cast<uint32_t>(numFrames * 1000ull / m_wav->sampleRate);
    }

    uint32_t ReplayWAV::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayWAV::GetExtraInfo() const
    {
        std::string info;
        for (uint32_t i = 0; i < m_wav->metadataCount; i++)
        {
            if (m_wav->pMetadata[i].type & drwav_metadata_type_list_all_info_strings)
            {
                if (!info.empty())
                    info += "\n";
                if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_software)
                    info += "Software: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_copyright)
                    info += "Copyright: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_title)
                    info += "Title: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_artist)
                    info += "Artist: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_comment)
                    info += "Comment: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_date)
                    info += "Date: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_genre)
                    info += "Genre: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_album)
                    info += "Album: ";
                else if (m_wav->pMetadata[i].type & drwav_metadata_type_list_info_tracknumber)
                    info += "Track number: ";
                if (m_wav->pMetadata[i].data.infoText.pString[m_wav->pMetadata[i].data.infoText.stringLength - 1] == 0)
                    info += m_wav->pMetadata[i].data.infoText.pString;
                else
                    info.append(m_wav->pMetadata[i].data.infoText.pString, m_wav->pMetadata[i].data.infoText.stringLength);
            }
        }
        return info;
    }

    std::string ReplayWAV::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        char buf[128];
        const char* fmt;
        switch (m_wav->fmt.formatTag)
        {
        case DR_WAVE_FORMAT_PCM:
            fmt = "PCM";
            break;
        case DR_WAVE_FORMAT_ADPCM:
            fmt = "ADPCM";
            break;
        case DR_WAVE_FORMAT_IEEE_FLOAT:
            fmt = "IEEE FLOAT";
            break;
        case DR_WAVE_FORMAT_ALAW:
            fmt = "ALAW";
            break;
        case DR_WAVE_FORMAT_MULAW:
            fmt = "MULAW";
            break;
        case DR_WAVE_FORMAT_DVI_ADPCM:
            fmt = "DVI ADPCM";
            break;
        case DR_WAVE_FORMAT_EXTENSIBLE:
            fmt = "EXTENSIBLE";
            break;
        default:
            fmt = "???";
        }
        sprintf(buf, "%dbit %s - %dHz\n", m_wav->bitsPerSample, fmt, m_wav->sampleRate);
        info += buf;
        info += "dr_wav " DRWAV_VERSION_STRING;
        return info;
    }
}
// namespace rePlayer