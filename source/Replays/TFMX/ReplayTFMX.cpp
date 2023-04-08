#include "ReplayTFMX.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

extern "C"
{
#include "TFMX/tfmx.h"
#include "TFMX/tfmx_iface.h"
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::TFMX,
        .name = "TFMX",
        .extensions = "tfx;tfm",
        .about = "TFMXPlay 1.0.2\nCopyright (c) 1996 Jonathan H. Pickard",
        .init = ReplayTFMX::Init,
        .load = ReplayTFMX::Load,
        .displaySettings = ReplayTFMX::DisplaySettings,
        .editMetadata = ReplayTFMX::Settings::Edit
    };

    bool ReplayTFMX::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayTFMXStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayTFMXSurround");
        window.RegisterSerializedData(ms_filter, "ReplayTFMXFilter");
        window.RegisterSerializedData(ms_resampling, "ReplayTFMXResampling");

        return false;
    }

    Replay* ReplayTFMX::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto data = stream->Read();
        if (memcmp(data.Items(), "TFMX-MOD", 8) != 0)
            return nullptr;

        auto smplOfs = *reinterpret_cast<const uint32_t*>(data.Items() + 8);
        auto smplSize = *reinterpret_cast<const uint32_t*>(data.Items() + 12) - smplOfs;

        TfmxData mdat{ const_cast<uint8_t*>(data.Items()) + 20, smplOfs - 20, 0 };
        TfmxData smpl{ const_cast<uint8_t*>(data.Items()) + smplOfs, smplSize, 0 };
        TfmxState* state = new TfmxState();
        TfmxState_init(state);
        state->outRate = kSampleRate;
        if (LoadTFMXFile(state, &mdat, &smpl) < 0)
        {
            TFMXQuit(state);
            delete state;
            return nullptr;
        }
        for (int32_t i = 0; i < state->nSongs; i++)
        {
            state->loops = 1;
            TFMXSetSubSong(state, i);

            while (state->mdb.PlayerEnable && state->hasUnsuportedCommands == 0)
                player_tfmxIrqIn(state);

            if (state->hasUnsuportedCommands)
            {
                Log::Warning("TFMX: unsupported macro detected, skipping \"%s\"\n", stream->GetName().c_str());

                TFMXQuit(state);
                delete state;
                return nullptr;
            }
        }

        return new ReplayTFMX(state);
    }

    bool ReplayTFMX::DisplaySettings()
    {
        bool changed = false;
        if (ImGui::CollapsingHeader("TFMX 1.0.2", ImGuiTreeNodeFlags_None))
        {
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PushID("TFMX");
            changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
            const char* const surround[] = { "Stereo", "Surround" };
            changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
            const char* const filters[] = { "Off", "Low", "Medium", "High" };
            changed |= ImGui::Combo("Filter", &ms_filter, filters, _countof(filters));
            const char* const resampling[] = { "Disable", "Enable" };
            changed |= ImGui::Combo("Resampling", &ms_resampling, resampling, _countof(resampling));
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PopID();
        }
        return changed;
    }

    void ReplayTFMX::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "Filter: Off", "Filter: Low", "Filter: Medium", "Filter: High");
        ComboOverride("Resampling", GETSET(entry, overrideResampling), GETSET(entry, resampling),
            ms_resampling, "No Resampling", "Resampling");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayTFMX::ms_stereoSeparation = 100;
    int32_t ReplayTFMX::ms_surround = 1;
    int32_t ReplayTFMX::ms_filter = 2;
    int32_t ReplayTFMX::ms_resampling = 1;

    ReplayTFMX::~ReplayTFMX()
    {
        TFMXQuit(m_state);
        delete m_state;
    }

    ReplayTFMX::ReplayTFMX(TfmxState* state)
        : Replay(eExtension::_tfm, eReplay::TFMX)
        , m_state(state)
        , m_surround(kSampleRate)
    {}

    uint32_t ReplayTFMX::Render(StereoSample* output, uint32_t numSamples)
    {
        auto samplesToRender = numSamples;
        while (numSamples)
        {
            if (m_remainingSamples > 0)
            {
                auto sampleToCopy = Min(numSamples, m_remainingSamples);

                auto sampleOffset = m_state->blocksize - m_remainingSamples;

                output = output->Convert(m_surround, m_samples + sampleOffset * 2, sampleToCopy, m_stereoSeparation);

                numSamples -= sampleToCopy;
                m_remainingSamples -= sampleToCopy;
            }
            else
            {
                if (m_loops != m_state->loops)
                {
                    if (samplesToRender == numSamples)
                        m_loops = m_state->loops;
                    return samplesToRender - numSamples;
                }
                do
                {
                    m_samples = reinterpret_cast<const float*>(tfmx_get_next_buffer(m_state));
                    if (!m_samples && tfmx_try_to_make_block(m_state) < 0)
                        return samplesToRender - numSamples;
                } while (!m_samples);
                m_remainingSamples = m_state->blocksize;
            }
        }

        return samplesToRender - numSamples;
    }

    uint32_t ReplayTFMX::Seek(uint32_t timeInMs)
    {
        m_loops = m_state->loops = INT_MAX;
        TFMXSetSubSong(m_state, m_subsongIndex);
        m_surround.Reset();

        uint64_t pos = 0;
        uint64_t end = timeInMs;
        end *= GetSampleRate();
        end /= 1000;
        while (pos < end)
        {
            player_tfmxIrqIn(m_state);
            auto nb = (m_state->eClocks * (m_state->outRate / 2));
            m_state->eRem += (nb % 357955);
            nb /= 357955;
            if (m_state->eRem > 357955) {
                nb++;
                m_state->eRem -= 357955;
            }
            pos += nb;
        }
        return uint32_t(pos * 1000 / GetSampleRate());
    }

    void ReplayTFMX::ResetPlayback()
    {
        m_loops = m_state->loops = INT_MAX;
        TFMXSetSubSong(m_state, m_subsongIndex);
        m_surround.Reset();
    }

    void ReplayTFMX::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        m_state->plugin_cfg.filt = (settings && settings->overrideFilter) ? settings->filter : ms_filter;
        m_state->plugin_cfg.over = (settings && settings->overrideResampling) ? settings->resampling : ms_resampling;
    }

    void ReplayTFMX::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayTFMX::GetDurationMs() const
    {
        return player_tfmxGetDuration(m_state);
    }

    uint32_t ReplayTFMX::GetNumSubsongs() const
    {
        return uint32_t(TFMXGetSubSongs(m_state));
    }

    std::string ReplayTFMX::GetExtraInfo() const
    {
        std::string metadata;
        for (uint32_t i = 0; i < 6; ++i)
        {
            char txt[48];
            sprintf(txt, "%40.40s\n", m_state->mdat_header.text[i]);
            metadata += txt;
        }
        return metadata;
    }

    std::string ReplayTFMX::GetInfo() const
    {
        std::string info;
        info = m_state->multimode ? "7 channels\n" : "4 channels\n";
        if (m_state->multimode)
            info += "TFMX 7V\n";
        else if (m_state->isPro)
            info += "TFMX Pro\n";
        else
            info += "TFMX 1.5\n";
        info += "TFMXPlay 1.0.2";
        return info;
    }
}
// namespace rePlayer