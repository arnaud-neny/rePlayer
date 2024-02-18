#include "ReplaySoundMon.h"
#include "BSPlay/SoundMonModule.h"
#include "BSPlay/SoundMonPlayer.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SoundMon,
        .name = "SoundMon",
        .extensions = "bp;bp3;bs",
        .about = "BSPlay 1.1\nBrian Postma",
        .settings = "SoundMon",
        .init = ReplaySoundMon::Init,
        .load = ReplaySoundMon::Load,
        .displaySettings = ReplaySoundMon::DisplaySettings,
        .editMetadata = ReplaySoundMon::Settings::Edit
    };

    bool ReplaySoundMon::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplaySoundMonStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplaySoundMonSurround");

        return false;
    }

    Replay* ReplaySoundMon::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024)
            return nullptr;
        auto data = stream->Read();
        auto module = SoundMon::Module::Load(data.Items(), data.Size());
        if (!module)
            return nullptr;

        return new ReplaySoundMon(module);
    }

    bool ReplaySoundMon::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplaySoundMon::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplaySoundMon::ms_stereoSeparation = 100;
    int32_t ReplaySoundMon::ms_surround = 1;

    ReplaySoundMon::~ReplaySoundMon()
    {
        delete m_player;
        m_module->Release();
    }

    ReplaySoundMon::ReplaySoundMon(SoundMon::Module* module)
        : Replay(module->GetVersion() == 3 ? eExtension::_bp3 : eExtension::_bp, eReplay::SoundMon)
        , m_module(module)
        , m_player(new SoundMon::Player(module, kSampleRate))
        , m_surround(kSampleRate)
    {}

    uint32_t ReplaySoundMon::Render(StereoSample* output, uint32_t numSamples)
    {
        numSamples = m_player->Render(reinterpret_cast<float*>(output), numSamples);
        output->Convert(m_surround, numSamples);
        return numSamples;
    }

    uint32_t ReplaySoundMon::Seek(uint32_t timeInMs)
    {
        timeInMs = m_player->Seek(timeInMs);
        m_surround.Reset();
        return timeInMs;
    }

    void ReplaySoundMon::ResetPlayback()
    {
        m_player->Stop();
        m_surround.Reset();
    }

    void ReplaySoundMon::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_player->SetStereoSeparation(((settings && settings->overrideStereoSeparation) ? 100 - settings->stereoSeparation : ms_stereoSeparation) / 100.f);
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplaySoundMon::SetSubsong(uint16_t subsongIndex)
    {
        m_player->SetSubsong(static_cast<uint8_t>(subsongIndex));
    }

    uint32_t ReplaySoundMon::GetDurationMs() const
    {
        return m_player->GetDuration();
    }

    uint32_t ReplaySoundMon::GetNumSubsongs() const
    {
        return m_player->GetNumSubsongs();
    }

    std::string ReplaySoundMon::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_module->GetSongName();
        for (uint16_t i = 0; i <= SoundMon::kNumInstr; i++)
        {
            metadata += "\n";
            metadata += m_module->GetInstrumentName(i);
        }
        return metadata;
    }

    std::string ReplaySoundMon::GetInfo() const
    {
        std::string info;

        char buf[32];
        sprintf(buf, "%d", SoundMon::kVoices);
        info += buf;
        info += " channels\n";

        info += "SoundMon ";
        info += '0' + m_module->GetVersion();
        info += ".0\nBSPlay 1.1";//1.1 instead of 1.0 because I fixed it myself :P

        return info;
    }
}
// namespace rePlayer