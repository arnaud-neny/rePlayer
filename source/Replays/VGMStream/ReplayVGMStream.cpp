// vgmstream customed files:
// - coding/coding.h
// - coding/ffmpeg_decoder.c
// - formats.c
// - vgmstream.h
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
        .about = "vgmstream " VGMSTREAM_VERSION "\nCopyright (c) 2008-2019 Adam Gashlin, Fastelbja, Ronny Elfert, bnnm\n"
            "Christopher Snowhill, NicknineTheEagle, bxaimc\n"
            "Thealexbarney, CyberBotX, et al\n"
            "Portions Copyright (c) 2004-2008, Marko Kreen\n"
            "Portions Copyright (c) 2001-2007  jagarl/Kazunori Ueno <jagarl@creator.club.ne.jp>\n"
            "Portions Copyright (c) 1998, Justin Frankel/Nullsoft Inc.\n"
            "Portions Copyright (c) 2006 Nullsoft, Inc.\n"
            "Portions Copyright (c) 2005-2007 Paul Hsieh",
        .init = ReplayVGMStream::Init,
        .release = ReplayVGMStream::Release,
        .load = ReplayVGMStream::Load
    };

    bool ReplayVGMStream::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            size_t size = 0;
            auto* formats = vgmstream_get_formats(&size);
            std::string extensions;
            while (size--)
            {
                extensions += *formats++;
                extensions += ';';
            }
            extensions += "m4a;m4v;mov;wma";
            auto ext = new char[extensions.size() + 1];
            memcpy(ext, extensions.c_str(), extensions.size() + 1);
            g_replayPlugin.extensions = ext;
        }

        return false;
    }

    void ReplayVGMStream::Release()
    {
        delete[] g_replayPlugin.extensions;
    }

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

        MediaType mediaType(eExtension::Unknown, eReplay::VGMStream);
        auto* ext = filename_extension(stream->GetName().c_str());
        if (ext)
            mediaType = { ext, eReplay::VGMStream };
        VGMSTREAM* vgmstream = nullptr;
        if (mediaType.ext == eExtension::_txth)
        {
            std::string otherFileName;
            otherFileName.append(stream->GetName().c_str(), ext - 1);
            if (auto otherStream = stream->Open(otherFileName))
            {
                streamFile->stream = std::move(otherStream);
                vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
                if (!vgmstream)
                    streamFile->stream = stream;
            }
        }
        if (!vgmstream)
            vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
        if (vgmstream)
        {
            if (vgmstream->meta_type == meta_TXTH)
                mediaType.ext = eExtension::_txth;
/* keep this when there is a need for it (another site scrapping?)
            else
            {
                char coding[128] = {};
                get_vgmstream_coding_description(vgmstream, coding, sizeof(coding));
                char format[128] = {};
                get_vgmstream_ffmpeg_format(vgmstream, format, sizeof(format));
                if (vgmstream->config.is_txtp)
                    mediaType.ext = eExtension::_txtp;
                else switch (vgmstream->meta_type)
                {
                case meta_AAX:
                    mediaType.ext = eExtension::_aax; break;
                case meta_ADS:
                    mediaType.ext = vgmstream->loop_flag ? eExtension::_ads : eExtension::_ss2; break;
                case meta_ADX_03:
                case meta_ADX_04:
                case meta_ADX_05:
                    mediaType.ext = eExtension::_adx; break;
                case meta_AFC:
                    mediaType.ext = eExtension::_afc; break;
                case meta_AST:
                    mediaType.ext = eExtension::_ast; break;
                case meta_AUS:
                    mediaType.ext = eExtension::_aus; break;
                case meta_BINK:
                    mediaType.ext = eExtension::_bik; break;
                case meta_BNS:
                    mediaType.ext = eExtension::_bns; break;
                case meta_CSTM:
                    mediaType.ext = eExtension::_bcstm; break;
                case meta_CWAV:
                    mediaType.ext = eExtension::_bcwav; break;
                case meta_EA_SCHL:
                    mediaType.ext = (vgmstream->coding_type == coding_EA_XA_int || vgmstream->coding_type == coding_EA_MT || vgmstream->coding_type == coding_EA_XA_V2) ? eExtension::_sng : eExtension::_asf; break;
                case meta_EXST:
                    mediaType.ext = eExtension::_sts; break;
                case meta_FFMPEG:
                    if (strcmp(coding, "Windows Media Audio 2") == 0)
                        mediaType.ext = eExtension::_wma;
                    else if (strcmp(coding, "TAK (Tom's lossless Audio Kompressor)") == 0)
                        mediaType.ext = eExtension::_tak;
                    else if (strstr(coding, "ATRAC3+") && strcmp(format, "Sony OpenMG audio") == 0)
                        mediaType.ext = eExtension::_oma;
                    break;
                case meta_FSB1:
                case meta_FSB2:
                case meta_FSB3:
                case meta_FSB4:
                case meta_FSB5:
                    mediaType.ext = eExtension::_fsb; break;
                case meta_GENH:
                    mediaType.ext = eExtension::_genh; break;
                case meta_HCA:
                    mediaType.ext = eExtension::_hca; break;
                case meta_MIB_MIH:
                    mediaType.ext = eExtension::_mib; break;
                case meta_MSF:
                    mediaType.ext = eExtension::_msf; break;
                case meta_MTAF:
                    mediaType.ext = eExtension::_mtaf; break;
                case meta_MUL:
                    mediaType.ext = vgmstream->coding_type == coding_PSX ? eExtension::_emff : eExtension::_mul; break;
                case meta_MUSC:
                    mediaType.ext = eExtension::_musc; break;
                case meta_PS_HEADERLESS:
                    mediaType.ext = eExtension::_mib; break;
                case meta_RAW_INT:
                    mediaType.ext = eExtension::_int; break;
                case meta_RIFF_WAVE:
                    mediaType.ext = vgmstream->coding_type == coding_XBOX_IMA ? eExtension::_lwav : vgmstream->coding_type == coding_L5_555 ? eExtension::_mwv : strstr(coding, "ATRAC3+") ? eExtension::_at3 : eExtension::_wav; break;
                case meta_RIFF_WAVE_MWV:
                    mediaType.ext = eExtension::_mwv; break;
                case meta_RIFF_WAVE_POS:
                    mediaType.ext = eExtension::_pos; break;
                case meta_RIFF_WAVE_smpl:
                    mediaType.ext = strstr(coding, "ATRAC3+") ? eExtension::_at3 : strstr(coding, "ATRAC9") ? eExtension::_at9 : eExtension::_wav; break;
                case meta_RSTM_ROCKSTAR:
                    mediaType.ext = eExtension::_rstm; break;
                case meta_RSTM:
                    mediaType.ext = eExtension::_brstm; break;
                case meta_RWAV:
                    mediaType.ext = eExtension::_rwav; break;
                case meta_SQEX_SAB:
                    mediaType.ext = eExtension::_sab; break;
                case meta_SQEX_SCD:
                    mediaType.ext = eExtension::_scd; break;
                case meta_THP:
                    mediaType.ext = eExtension::_dsp; break;
                case meta_UBI_JADE:
                    mediaType.ext = vgmstream->loop_flag ? eExtension::_wam : eExtension::_wac; break;
                case meta_VAG:
                case meta_VAG_custom:
                    mediaType.ext = eExtension::_vag; break;
                case meta_VGS:
                    mediaType.ext = eExtension::_vgs; break;
                case meta_VPK:
                    mediaType.ext = eExtension::_vpk; break;
                case meta_WWISE_RIFF: // this one is borked (logg == wem)
                    mediaType.ext = vgmstream->num_streams > 1 ? eExtension::_bnk : vgmstream->coding_type == coding_VORBIS_custom ? eExtension::_logg : eExtension::_wem; break;
                case meta_XA:
                    mediaType.ext = eExtension::_xa; break;
                case meta_XMA_RIFF:
                    mediaType.ext = vgmstream->num_streams > 1 ? eExtension::_bnk : eExtension::_xma; break;
                case meta_XVAG:
                    mediaType.ext = eExtension::_xvag; break;
                }
            }
*/
            return new ReplayVGMStream(streamFile, vgmstream, mediaType);
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

    ReplayVGMStream::ReplayVGMStream(StreamFile* streamFile, VGMSTREAM* vgmstream, MediaType mediaType)
        : Replay(mediaType)
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
        if (!m_vgmstream)
            return 0;

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
        if (m_vgmstream)
        {
            seek_vgmstream(m_vgmstream, uint32_t((uint64_t(timeInMs) * m_vgmstream->sample_rate) / 1000ull));
            m_loopCount = m_vgmstream->loop_count;
        }
        return timeInMs;
    }

    void ReplayVGMStream::ResetPlayback()
    {
        if (m_vgmstream)
        {
            reset_vgmstream(m_vgmstream);
            m_loopCount = m_vgmstream->loop_count;
        }
    }

    void ReplayVGMStream::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayVGMStream::SetSubsong(uint32_t subsongIndex)
    {
        if (m_subsongIndex != subsongIndex)
        {
            close_vgmstream(m_vgmstream);

            m_streamFile->stream_index = subsongIndex + 1;
            m_vgmstream = init_vgmstream_from_STREAMFILE(m_streamFile);

            if (m_vgmstream)
            {
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
            }
            m_subsongIndex = subsongIndex;
        }
    }

    uint32_t ReplayVGMStream::GetDurationMs() const
    {
        return m_vgmstream ? uint32_t(uint64_t((vgmstream_get_samples(m_vgmstream) + m_vgmstream->sample_rate / 2) * 1000ull) / m_vgmstream->sample_rate) : 0;
    }

    uint32_t ReplayVGMStream::GetNumSubsongs() const
    {
        return m_vgmstream ? uint32_t(m_vgmstream->num_streams > 1 ? m_vgmstream->num_streams : 1) : 1;
    }

    std::string ReplayVGMStream::GetExtraInfo() const
    {
        std::string info;
        if (m_vgmstream)
        {
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
            if (m_vgmstream->meta_type == meta_FFMPEG)
            {
                get_vgmstream_ffmpeg_format(m_vgmstream, temp, sizeof(temp));
                if (temp[0])
                {
                    if (!info.empty() && info.back() != '\n')
                        info += '\n';
                    info += "FFmpeg format: ";
                    info += temp;
                }
            }
        }
        return info;
    }

    std::string ReplayVGMStream::GetInfo() const
    {
        std::string info;
        char buf[256];
        if (m_vgmstream)
        {
            if (m_vgmstream->channels <= 1)
                info = "1 channel\n";
            else
            {
                sprintf(buf, "%d channels\n", m_vgmstream->channels);
                info = buf;
            }
            if (m_vgmstream->meta_type != meta_FFMPEG)
                get_vgmstream_meta_description(m_vgmstream, buf, sizeof(buf));
            else
                get_vgmstream_ffmpeg_format(m_vgmstream, buf, sizeof(buf));
            info += buf;
            info += "/";
            get_vgmstream_coding_description(m_vgmstream, buf, sizeof(buf));
            info += buf;
        }
        else
        {
            sprintf(buf, "Failed to load\nSubsong %u", m_subsongIndex + 1);
            info = buf;
        }
        info += "\nvgmstream " VGMSTREAM_VERSION;
        return info;
    }
}
// namespace rePlayer