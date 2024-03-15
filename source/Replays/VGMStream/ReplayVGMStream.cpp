#include "ReplayVGMStream.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

extern "C" {
#include "vgmstream/version.h"
#include "vgmstream/src/api.h"
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::VGMStream,
        .name = "vgmstream",
        .extensions = "adx;afc;asf;ast;bcstm;bcwav;bns;brstm;dsp;genh;hca;lwav;msf;rwav;scd;wma",
        .about = "vgmstream " VGMSTREAM_VERSION "\nCopyright (c) 2008-2019 Adam Gashlin, Fastelbja, Ronny Elfert, bnnm\n"
            "Christopher Snowhill, NicknineTheEagle, bxaimc\n"
            "Thealexbarney, CyberBotX, et al\n"
            "Portions Copyright (c) 2004-2008, Marko Kreen\n"
            "Portions Copyright (c) 2001-2007  jagarl/Kazunori Ueno <jagarl@creator.club.ne.jp>\n"
            "Portions Copyright (c) 1998, Justin Frankel/Nullsoft Inc.\n"
            "Portions Copyright (c) 2006 Nullsoft, Inc.\n"
            "Portions Copyright (c) 2005-2007 Paul Hsieh",
        .load = ReplayVGMStream::Load
    };

    Replay* ReplayVGMStream::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* streamFile = new StreamFile;
        streamFile->read = reinterpret_cast<decltype(streamFile->read)>(StreamFileRead);
        streamFile->get_size = reinterpret_cast<decltype(streamFile->get_size)>(StreamFileGetSize);
        streamFile->get_offset = reinterpret_cast<decltype(streamFile->get_offset)>(StreamFileGetOffset);
        streamFile->get_name = reinterpret_cast<decltype(streamFile->get_name)>(StreamFileGetName);
        streamFile->open = reinterpret_cast<decltype(streamFile->open)>(StreamFileOpen);
        streamFile->close = reinterpret_cast<decltype(streamFile->close)>(StreamFileClose);
        streamFile->stream_index = 1;
        streamFile->stream = stream;

        if (auto* vgmstream = init_vgmstream_from_STREAMFILE(streamFile))
        {
            auto ext = eExtension::Unknown;
            switch (vgmstream->meta_type)
            {
            case meta_ADX_03:
            case meta_ADX_04:
                ext = eExtension::_adx; break;
            case meta_AFC:
                ext = eExtension::_afc; break;
            case meta_AST:
                ext = eExtension::_ast; break;
            case meta_BNS:
                ext = eExtension::_bns; break;
            case meta_CSTM:
                ext = eExtension::_bcstm; break;
            case meta_CWAV:
                ext = eExtension::_bcwav; break;
            case meta_EA_SCHL:
                ext = eExtension::_asf; break;
//             case meta_FFMPEG:
//                 ext = eExtension::_wma; break;
            case meta_GENH:
                ext = eExtension::_genh; break;
            case meta_HCA:
                ext = eExtension::_hca; break;
            case meta_MSF:
                ext = eExtension::_msf; break;
            case meta_RIFF_WAVE:
                ext = vgmstream->coding_type == coding_XBOX_IMA ? eExtension::_lwav : eExtension::_wav; break;
            case meta_RSTM:
                ext = eExtension::_brstm; break;
            case meta_RWAV:
                ext = eExtension::_rwav; break;
            case meta_SQEX_SCD:
                ext = eExtension::_scd; break;
            case meta_THP:
                ext = eExtension::_dsp; break;
            }
            return new ReplayVGMStream(streamFile, vgmstream, ext);
        }
        delete streamFile;
        return nullptr;
    }

    size_t ReplayVGMStream::StreamFileRead(StreamFile* sf, uint8_t* dst, offv_t offset, size_t length)
    {
        sf->stream->Seek(offset, io::Stream::kSeekBegin);
        return sf->stream->Read(dst, length);
    }

    size_t ReplayVGMStream::StreamFileGetSize(StreamFile* sf)
    {
        return sf->stream->GetSize();
    }

    offv_t ReplayVGMStream::StreamFileGetOffset(StreamFile* sf)
    {
        return sf->stream->GetPosition();
    }

    void ReplayVGMStream::StreamFileGetName(StreamFile* sf, char* name, size_t name_size)
    {
        auto& sfName = sf->stream->GetName();
        memcpy(name, sfName.c_str(), Min(name_size, sfName.size() + 1));
    }

    ReplayVGMStream::StreamFile* ReplayVGMStream::StreamFileOpen(StreamFile* sf, const char* const filename, size_t buf_size)
    {
        auto stream = sf->stream->Open(filename);
        if (stream.IsValid())
        {
            (void)buf_size;
            auto* streamFile = new StreamFile;
            streamFile->read = reinterpret_cast<decltype(streamFile->read)>(StreamFileRead);
            streamFile->get_size = reinterpret_cast<decltype(streamFile->get_size)>(StreamFileGetSize);
            streamFile->get_offset = reinterpret_cast<decltype(streamFile->get_offset)>(StreamFileGetOffset);
            streamFile->get_name = reinterpret_cast<decltype(streamFile->get_name)>(StreamFileGetName);
            streamFile->open = reinterpret_cast<decltype(streamFile->open)>(StreamFileOpen);
            streamFile->close = reinterpret_cast<decltype(streamFile->close)>(StreamFileClose);
            streamFile->stream = stream;
            return streamFile;
        }
        return nullptr;
    }

    void ReplayVGMStream::StreamFileClose(StreamFile* sf)
    {
        delete sf;
    }

    ReplayVGMStream::~ReplayVGMStream()
    {
        close_vgmstream(m_vgmstream);
        delete m_streamFile;
    }

    ReplayVGMStream::ReplayVGMStream(StreamFile* streamFile, VGMSTREAM* vgmstream, eExtension ext)
        : Replay(ext, eReplay::VGMStream)
        , m_streamFile(streamFile)
        , m_vgmstream(vgmstream)
        , m_loopCount(vgmstream->loop_count)
    {
        vgmstream_cfg_t vcfg = {};
        vcfg.allow_play_forever = 1;
        vcfg.play_forever = 1;
        vcfg.loop_count = 1;
        vcfg.fade_time = 0;
        vcfg.fade_delay = 0;
        vcfg.ignore_loop = 0;
        vgmstream_apply_config(vgmstream, &vcfg);

        vgmstream_mixing_autodownmix(vgmstream, 2);
        vgmstream_mixing_enable(vgmstream, 32768, nullptr, nullptr);
    }

    uint32_t ReplayVGMStream::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_vgmstream->loop_count != m_loopCount)
        {
            // a little bit lame and random but the vgmstream api is too poor to handle perfect loop time
            // and I'm too lazy to update it at my convenience
            m_loopCount = m_vgmstream->loop_count;
            return 0;
        }
        auto* buf = m_vgmstream->channels > 2 ? new int16_t[numSamples * m_vgmstream->channels] : (reinterpret_cast<int16_t*>(output + numSamples) - numSamples * m_vgmstream->channels);
        auto numRendered = render_vgmstream(buf, int32_t(numSamples), m_vgmstream);
        if (m_vgmstream->channels == 1)
        {
            output->ConvertMono(buf, numSamples);
        }
        else
        {
            output->Convert(buf, numRendered);
            if (m_vgmstream->channels > 2)
                delete[] buf;
        }
        return numRendered;
    }

    uint32_t ReplayVGMStream::Seek(uint32_t timeInMs)
    {
        seek_vgmstream(m_vgmstream, uint32_t((uint64_t(timeInMs) * m_vgmstream->sample_rate) / 1000ull));
        m_loopCount = m_vgmstream->loop_count;
        return timeInMs;
    }

    void ReplayVGMStream::ResetPlayback()
    {
        reset_vgmstream(m_vgmstream);
        m_loopCount = m_vgmstream->loop_count;
    }

    void ReplayVGMStream::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayVGMStream::SetSubsong(uint16_t subsongIndex)
    {
        if (m_subsongIndex != subsongIndex)
        {
            close_vgmstream(m_vgmstream);

            m_streamFile->stream_index = subsongIndex + 1;
            m_vgmstream = init_vgmstream_from_STREAMFILE(m_streamFile);

            vgmstream_cfg_t vcfg = {};
            vcfg.allow_play_forever = 1;
            vcfg.play_forever = 1;
            vcfg.loop_count = 1;
            vcfg.fade_time = 0;
            vcfg.fade_delay = 0;
            vcfg.ignore_loop = 0;
            vgmstream_apply_config(m_vgmstream, &vcfg);

            vgmstream_mixing_autodownmix(m_vgmstream, 2);
            vgmstream_mixing_enable(m_vgmstream, 32768, nullptr, nullptr);

            m_loopCount = m_vgmstream->loop_count;
            m_subsongIndex = subsongIndex;
        }
    }

    uint32_t ReplayVGMStream::GetDurationMs() const
    {
        return uint32_t(uint64_t((vgmstream_get_samples(m_vgmstream) + m_vgmstream->sample_rate / 2) * 1000ull) / m_vgmstream->sample_rate);
    }

    uint32_t ReplayVGMStream::GetNumSubsongs() const
    {
        return uint32_t(m_vgmstream->num_streams > 1 ? m_vgmstream->num_streams : 1);
    }

    std::string ReplayVGMStream::GetExtraInfo() const
    {
        std::string info;

        if (m_vgmstream->stream_name != NULL && strlen(m_vgmstream->stream_name) > 0)
        {
            info += "title: ";
            info += m_vgmstream->stream_name;
        }

        char temp[2048];
        describe_vgmstream(m_vgmstream, temp, sizeof(temp));
        if (temp[0])
        {
            if (!info.empty())
                info += '\n';
            info += temp;
        }

        return info;
    }

    std::string ReplayVGMStream::GetInfo() const
    {
        std::string info;
        char buf[256];
        if (m_vgmstream->channels <= 1)
            info = "1 channel\n";
        else
        {
            sprintf(buf, "%d channels\n", m_vgmstream->channels);
            info = buf;
        }
        get_vgmstream_meta_description(m_vgmstream, buf, sizeof(buf));
        info += buf;
        info += "/";
        get_vgmstream_coding_description(m_vgmstream, buf, sizeof(buf));
        info += buf;
        info += "\nvgmstream " VGMSTREAM_VERSION;
        return info;
    }
}
// namespace rePlayer