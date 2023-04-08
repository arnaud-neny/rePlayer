#include "ReplayV2.h"
#include "V2/sounddef.h"
#include "V2/v2mconv.h"
#include "V2/v2mplayer.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::V2,
        .name = "Farbrausch ViruZ II",
        .extensions = "v2m",
        .about = "Farbrausch ViruZ II\nCopyright (c) 2000-2008 Tammo \"kb\" Hinrichs",
        .init = ReplayV2::Init,
        .load = ReplayV2::Load,
        .displaySettings = ReplayV2::DisplaySettings,
        .editMetadata = ReplayV2::Settings::Edit
    };

    bool ReplayV2::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_isCoreSynth, "ReplayV2CoreSynth");

        return false;
    }

    Replay* ReplayV2::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();
        if (data[2] != 0 || data[3] != 0)
            return nullptr;

        sdInit();

        uint8_t* tune{ nullptr };
        int newSize{ 0 };
        ConvertV2M(data.Items(), static_cast<int>(data.Size()), &tune, &newSize);

        sdClose();

        if (tune == nullptr)
            return nullptr;

        auto settings = metadata.Find<Settings>();
        auto player = new V2MPlayer((settings && settings->overrideCoreSynth) ? settings->coreSynth != 0 : ms_isCoreSynth);
        if (player->Open(tune, kSampleRate) == sFALSE)
        {
            player->Close();
            delete player;
            delete[] tune;
            return nullptr;
        }

        return new ReplayV2(player, tune);
    }

    bool ReplayV2::DisplaySettings()
    {
        bool changed = false;
        if (ImGui::CollapsingHeader("Farbrausch ViruZ II", ImGuiTreeNodeFlags_None))
        {
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PushID("V2");

            const char* const synths[] = { "x86", "Core" };
            int index = ms_isCoreSynth ? 1 : 0;
            changed |= ImGui::Combo("Synth engine", &index, synths, _countof(synths));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("V2 Synth x86 is \"emulating\" original assembler code.\nV2 Synth Core is a lot fast but it isn't playing properly some tunes.");
            ms_isCoreSynth = index == 1;

            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PopID();
        }
        return changed;
    }

    void ReplayV2::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("CoreSynth", GETSET(entry, overrideCoreSynth), GETSET(entry, coreSynth),
            ms_isCoreSynth, "x86 Synth", "Core Synth");

        context.metadata.Update(entry, entry->value == 0);
    }

    bool ReplayV2::ms_isCoreSynth = false;

    ReplayV2::~ReplayV2()
    {
        m_player->Close();
        delete m_player;
        delete[] m_tune;
    }

    ReplayV2::ReplayV2(V2MPlayer* player, uint8_t* tune)
        : Replay(eExtension::_v2m, eReplay::V2)
        , m_player(player)
        , m_tune(tune)
    {}

    uint32_t ReplayV2::Render(StereoSample* output, uint32_t numSamples)
    {
        if (!m_player->IsPlaying())
        {
            if (!m_hasEnded)
            {
                m_hasEnded = true;
                return 0;
            }
        }
        m_player->Render(reinterpret_cast<float*>(output), numSamples);
        return numSamples;
    }

    uint32_t ReplayV2::Seek(uint32_t timeInMs)
    {
        m_hasEnded = false;
        m_player->Play(timeInMs);
        return timeInMs;
    }

    void ReplayV2::ResetPlayback()
    {
        m_hasEnded = false;
        m_player->Play();
    }

    void ReplayV2::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayV2::SetSubsong(uint16_t /*subsongIndex*/)
    {
        m_player->Play();
    }

    uint32_t ReplayV2::GetDurationMs() const
    {
        int32_t* positions;
        auto numPositions = m_player->CalcPositions(&positions);
        auto length = static_cast<uint32_t>(positions[2 * (numPositions - 1)] + 2000);
        delete[] positions;
        return length;
    }

    uint32_t ReplayV2::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayV2::GetExtraInfo() const
    {
        return {};
    }

    std::string ReplayV2::GetInfo() const
    {
        char txt[32];
        sprintf(txt, "%d channels\n", m_player->GetNumChannels());
        std::string info = txt;
        info += "V2 Module File\n"
            "Farbrausch ViruZ II";
        info += m_player->IsCoreSynth() ? " (Core)" : " (x86)";
        return info;
    }
}
// namespace rePlayer