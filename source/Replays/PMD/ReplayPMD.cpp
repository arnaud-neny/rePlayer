#include "ReplayPMD.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::PMD,
        .name = "Professional Music Driver",
        .extensions = "m;m2;mz",
        .about = "Professional Music Driver (PMDWin 0.52)\nCopyright (c) 2023 C60",
        .settings = "PMDWin 0.52",
        .init = ReplayPMD::Init,
        .load = ReplayPMD::Load,
        .displaySettings = ReplayPMD::DisplaySettings,
        .editMetadata = ReplayPMD::Settings::Edit
    };

    bool ReplayPMD::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayPMDStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayPMDSurround");

        return false;
    }

    Replay* ReplayPMD::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* pmd = PMDWIN::Create(stream);
        pmd->init(nullptr);
        if (pmd->music_load((TCHAR*)stream->GetName().c_str()) == PMDWIN_OK)
        {
            auto data = stream->Read();
            char str[MAX_MEMOBUF];
            std::string info;

            int al = 1;
            while (pmd->getmemo3(str, (uint8_t*)data.Items(), data.NumItems<int>(), al++) && str[0])
            {
                int len = ::MultiByteToWideChar(932, 0, str, -1, nullptr, 0);
                if (!len)
                    continue;
                auto* wc = reinterpret_cast<wchar_t*>(_alloca(len * sizeof(wchar_t)));
                ::MultiByteToWideChar(932, 0, str, -1, wc, len);

                len = ::WideCharToMultiByte(CP_UTF8, 0, wc, -1, nullptr, 0, nullptr, nullptr);
                auto* c = reinterpret_cast<char*>(_alloca(len * sizeof(wchar_t)));
                ::WideCharToMultiByte(CP_UTF8, 0, wc, -1, c, len, nullptr, nullptr);

                if (!info.empty())
                    info += "\n";
                info += c;
            }

            return new ReplayPMD(pmd, std::move(info));
        }
        delete pmd;
        return nullptr;
    }

    bool ReplayPMD::DisplaySettings()
    {
        bool changed = false;
       changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
       const char* const surround[] = { "Stereo", "Surround" };
       changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayPMD::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

       SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
           ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
       ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
           ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayPMD::ms_stereoSeparation = 100;
    int32_t ReplayPMD::ms_surround = 0;

    ReplayPMD::~ReplayPMD()
    {
        delete m_pmd;
    }

    ReplayPMD::ReplayPMD(PMDWIN* pmd, std::string&& info)
        : Replay(eExtension::_m, eReplay::PMD)
        , m_surround(kSampleRate)
        , m_pmd(pmd)
        , m_info(std::move(info))
    {}

    uint32_t ReplayPMD::Render(core::StereoSample* output, uint32_t numSamples)
    {
        auto* buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        numSamples = m_pmd->getpcmdata(buf, numSamples);
        output->Convert(m_surround, buf, numSamples, m_stereoSeparation);
        return numSamples;
    }

    void ReplayPMD::ResetPlayback()
    {
        m_pmd->music_stop();
        m_pmd->music_start();
        m_surround.Reset();
    }

    void ReplayPMD::ApplySettings(const CommandBuffer metadata)
    {
       auto settings = metadata.Find<Settings>();
       m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
       m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayPMD::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayPMD::GetDurationMs() const
    {
        int length, loop;
        m_pmd->getlength(nullptr, &length, &loop);
        return length;

    }

    uint32_t ReplayPMD::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayPMD::GetExtraInfo() const
    {
        return m_info;
    }

    std::string ReplayPMD::GetInfo() const
    {
        std::string info;
        info = "2 channels\nPC-98";
        if (m_pmd->getopenwork()->use_p86)
            info += "/P86";
        info += "\nPMDWin 0.52";
        return info;
    }
}
// namespace rePlayer