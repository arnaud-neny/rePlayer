#include "ReplayFMP.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/StreamFile.h>
#include <RePlayer/Version.h>
#include <ReplayDll.h>

#include <version.h>

#include <filesystem>

#pragma comment(lib, "shlwapi")

thread_local core::io::Stream* tl_stream = nullptr;

extern "C"
{
    bool fmplayer_drum_rom_load(struct opna_drum* drum)
    {
        auto mainPath = std::filesystem::current_path() / "replays" REPLAYER_OS_STUB / "FMP" / "ym2608_adpcm_rom.bin";
        auto stream = core::io::StreamFile::Create(reinterpret_cast<const char*>(mainPath.u8string().c_str()));
        if (stream.IsValid() && stream->GetSize() == OPNA_ROM_SIZE)
        {
            char rom[OPNA_ROM_SIZE];
            stream->Read(rom, OPNA_ROM_SIZE);
            opna_drum_set_rom(drum, rom);
            return true;
        }
        return false;
    }
    void* fileread(const wchar_t* path, size_t maxsize, size_t* filesize, enum fmplayer_file_error* error)
    {
        std::filesystem::path p(path);
        auto stream = tl_stream->Open((const char*)p.generic_u8string().c_str());
        if (stream.IsValid())
        {
            auto size = stream->GetSize();
            if (maxsize && size > maxsize)
            {
                if (error) *error = FMPLAYER_FILE_ERR_BADFILE_SIZE;
                return nullptr;
            }
            *filesize = size;
            void* buf = malloc(size);
            stream->Read(buf, size);
            return buf;
        }
        if (error) *error = FMPLAYER_FILE_ERR_NOTFOUND;
        return nullptr;
    }
    unsigned opna_timer_tick(struct opna_timer* timer);
    unsigned opna_timer_mix_tick(struct opna_timer* timer, int16_t* buf, unsigned maxsamples);
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::FMP,
        .name = "FMPlayer",
        .extensions = "opi;ovi;ozi;m;m2;mz",
        .about = "FMPlayer (PC-98 FM driver emulation)\nCopyright (c) 2016 Takamichi Horikawa",
        .settings = "FMPlayer " FMPLAYER_VERSION_STR,
        .init = ReplayFMP::Init,
        .load = ReplayFMP::Load,
        .displaySettings = ReplayFMP::DisplaySettings,
        .editMetadata = ReplayFMP::Settings::Edit
    };

    bool ReplayFMP::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayFMPStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayFMPSurround");

        return false;
    }

    Replay* ReplayFMP::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto tl = Scope([&](){ tl_stream = stream; },[]() { tl_stream = nullptr; });
        enum fmplayer_file_error error;
        if (auto* fmfile = fmplayer_file_alloc(stream->GetName().c_str(), &error))
            return new ReplayFMP(stream, fmfile);
        return nullptr;
    }

    bool ReplayFMP::DisplaySettings()
    {
        bool changed = false;
       changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
       const char* const surround[] = { "Stereo", "Surround" };
       changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayFMP::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

       SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
           ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
       ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
           ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayFMP::ms_stereoSeparation = 100;
    int32_t ReplayFMP::ms_surround = 0;

    ReplayFMP::~ReplayFMP()
    {
        fmplayer_file_free(m_fmp.fmfile);
    }

    ReplayFMP::ReplayFMP(io::Stream* stream, fmplayer_file* fmfile)
        : Replay(eExtension::_opi, eReplay::FMP)
        , m_surround(kSampleRate)
        , m_stream(stream)
    {
        memset(&m_fmp, 0, sizeof(m_fmp));
        m_fmp.fmfile = fmfile;

        fmplayer_init_work_opna(&m_fmp.work, &m_fmp.ppz8, &m_fmp.opna, &m_fmp.timer, m_fmp.adpcmram);
        fmplayer_file_load(&m_fmp.work, fmfile, 1);

        if (m_fmp.fmfile->type == FMPLAYER_FILE_TYPE_FMP)
        {
            auto* fmp = (driver_fmp*)m_fmp.work.driver;
            if (fmp->ppz_name[0])
                m_mediaType.ext = eExtension::_ozi;
            else if (fmp->datainfo.partptr[FMP_DATA_FM_4] != 0xffFF
                || fmp->datainfo.partptr[FMP_DATA_FM_5] != 0xffFF
                || fmp->datainfo.partptr[FMP_DATA_FM_5] != 0xffFF)
                m_mediaType.ext = eExtension::_ovi;
        }
        else
            m_mediaType.ext = eExtension::_m;

        for (int i = 0;; ++i)
        {
            auto* comment = m_fmp.work.get_comment(&m_fmp.work, i);
            if (!comment)
                break;

            int len = ::MultiByteToWideChar(932, 0, comment, -1, nullptr, 0);
            if (!len)
                continue;
            auto* wc = reinterpret_cast<wchar_t*>(_alloca(len * sizeof(wchar_t)));
            ::MultiByteToWideChar(932, 0, comment, -1, wc, len);

            len = ::WideCharToMultiByte(CP_UTF8, 0, wc, -1, nullptr, 0, nullptr, nullptr);
            auto* c = reinterpret_cast<char*>(_alloca(len * sizeof(wchar_t)));
            ::WideCharToMultiByte(CP_UTF8, 0, wc, -1, c, len, nullptr, nullptr);

            if (!m_info.empty())
                m_info += "\n";
            m_info += c;
        }

    }

    uint32_t ReplayFMP::Render(core::StereoSample* output, uint32_t numSamples)
    {
        if (m_fmp.work.loop_cnt || !m_fmp.work.playing)
            m_fmp.work.loop_cnt = 0;

        auto* buffer = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        memset(buffer, 0, numSamples * sizeof(int16_t) * 2);
        auto remainingSamples = numSamples;
        do 
        {
            auto numRendered = opna_timer_mix_tick(&m_fmp.timer, buffer, remainingSamples);
            buffer += numRendered * 2;
            remainingSamples -= numRendered;
            if (m_fmp.work.loop_cnt)
                break;
        } while (m_fmp.work.playing && remainingSamples > 0);
        numSamples -= remainingSamples;
        buffer -= numSamples * 2;
        output->Convert(m_surround, buffer, numSamples, m_stereoSeparation);
        if (numSamples == 0)
            m_fmp.work.loop_cnt = 0;

        return numSamples;
    }

    void ReplayFMP::ResetPlayback()
    {
        fmplayer_file_free(m_fmp.fmfile);
        memset(&m_fmp, 0, sizeof(m_fmp));

        auto tl = Scope([&]() { tl_stream = m_stream; }, []() { tl_stream = nullptr; });
        enum fmplayer_file_error error;
        m_fmp.fmfile = fmplayer_file_alloc(m_stream->GetName().c_str(), &error);
        fmplayer_init_work_opna(&m_fmp.work, &m_fmp.ppz8, &m_fmp.opna, &m_fmp.timer, m_fmp.adpcmram);
        fmplayer_file_load(&m_fmp.work, m_fmp.fmfile, 1);

        m_surround.Reset();
    }

    void ReplayFMP::ApplySettings(const CommandBuffer metadata)
    {
       auto settings = metadata.Find<Settings>();
       m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
       m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayFMP::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayFMP::GetDurationMs() const
    {
        int64_t total_smpl = 0;
        while (m_fmp.work.playing)
        {
            total_smpl += opna_timer_tick((opna_timer*)&m_fmp.timer);
            if (m_fmp.work.loop_cnt)
                break;
        }
        return uint32_t(total_smpl * 1000ull / kSampleRate);
    }

    uint32_t ReplayFMP::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayFMP::GetExtraInfo() const
    {
        return m_info;
    }

    std::string ReplayFMP::GetInfo() const
    {
        std::string info;
        info = "2 channels\nPC-98 ";
        if (m_fmp.fmfile->type == FMPLAYER_FILE_TYPE_FMP)
        {
            info += "FMP ";
            if (m_mediaType.ext == eExtension::_opi)
                info += "OPN";
            else if (m_mediaType.ext == eExtension::_ovi)
                info += "OPNA";
            else //if (m_mediaType.ext == eExtension::_ozi)
                info += "OPNA PPZ";
        }
        else
            info += "PMD";
        info += "\nFMPlayer " FMPLAYER_VERSION_STR;
        return info;
    }
}
// namespace rePlayer