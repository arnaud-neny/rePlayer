#include "replayHively.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "hvl/hvl_replay.h"
#ifdef __cplusplus
}
#endif

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Hively,
        .name = "HivelyTracker",
        .extensions = "ahx;hvl",
        .about = "HivelyTracker 1.9\nCopyright (c) 2006-2018 Pete Gordon",
        .settings = "HivelyTracker 1.9",
        .init = ReplayHively::Init,
        .load = ReplayHively::Load,
        .displaySettings = ReplayHively::DisplaySettings,
        .editMetadata = ReplayHively::Settings::Edit
    };

    bool ReplayHively::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        hvl_InitReplayer();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayHivelyStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayHivelySurround");

        return false;
    }

    Replay* ReplayHively::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() < 8)
            return nullptr;

        auto data = stream->Read();
        auto module = hvl_ParseTune(data.Items(), static_cast<uint32_t>(data.Size()), kSampleRate, 2, [](size_t size) { return Alloc(size); }, [](void* ptr) { Free(ptr); });
        if (!module)
            return nullptr;

        const char* extension;
        if ((data[0] == 'T') &&
            (data[1] == 'H') &&
            (data[2] == 'X') &&
            (data[3] < 3))
        {
            extension = "ahx";
        }
        else/* if ((data[0] == 'H') &&
            (data[1] == 'V') &&
            (data[2] == 'L') &&
            (data[3] < 2))*/
        {
            extension = "hvl";
        }

        return new ReplayHively(module, extension);
    }

    bool ReplayHively::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplayHively::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayHively::ms_stereoSeparation = 100;
    int32_t ReplayHively::ms_surround = 1;

    ReplayHively::~ReplayHively()
    {
        hvl_FreeTune(m_module);
        delete[] m_tickData;
    }

    ReplayHively::ReplayHively(hvl_tune* module, const char* extension)
        : Replay(extension, eReplay::Hively)
        , m_module(module)
        , m_tickData(new int16_t[2 * kSampleRate / 50 / module->ht_SpeedMultiplier])
        , m_surround(kSampleRate)
    {}

    uint32_t ReplayHively::Render(StereoSample* output, uint32_t numSamples)
    {
        uint32_t maxSamples = kSampleRate / 50 / m_module->ht_SpeedMultiplier;
        uint32_t numRenderedSamples = 0;

        auto numAvailableSamples = m_numAvailableSamples;
        auto tickData = m_tickData + (maxSamples - numAvailableSamples) * 2;

        while (numSamples > 0)
        {
            auto numSamplesToConvert = numAvailableSamples <= numSamples ? numAvailableSamples : numSamples;

            auto surround = m_surround.Begin();
            for (uint32_t i = 0; i < numSamplesToConvert; i++)
            {
                *output++ = surround({ tickData[0] / 32767.0f , tickData[1] / 32767.0f });
                tickData += 2;
            }
            m_surround.End(surround);
            numRenderedSamples += numSamplesToConvert;
            numAvailableSamples -= numSamplesToConvert;
            numSamples -= numSamplesToConvert;

            if (numAvailableSamples == 0)
            {
                if (m_module->ht_SongEndReached)
                {
                    if (numRenderedSamples > 0)
                    {
                        m_numAvailableSamples = 0;
                        return numRenderedSamples;
                    }
                    else
                    {
                        m_module->ht_SongEndReached = 0;
                        return 0;
                    }
                }

                tickData -= maxSamples * 2;
                hvl_Decode(m_module, tickData, maxSamples);
                numAvailableSamples = maxSamples;
            }
        }

        m_numAvailableSamples = numAvailableSamples;

        return numRenderedSamples;
    }

    uint32_t ReplayHively::Seek(uint32_t timeInMs)
    {
        timeInMs = hvl_Seek(m_module, timeInMs);
        m_surround.Reset();
        return timeInMs;
    }

    void ReplayHively::ResetPlayback()
    {
        hvl_InitSubsong(m_module, m_subsongIndex);
        m_surround.Reset();
    }

    void ReplayHively::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        hvl_UpdateStereo(m_module, (settings && settings->overrideStereoSeparation) ? 100 - settings->stereoSeparation : ms_stereoSeparation);
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayHively::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        hvl_InitSubsong(m_module, subsongIndex);
    }

    uint32_t ReplayHively::GetDurationMs() const
    {
        return hvl_GetDuration(m_module);
    }

    uint32_t ReplayHively::GetNumSubsongs() const
    {
        return m_module->ht_SubsongNr > 0 ? m_module->ht_SubsongNr : 1;
    }

    std::string ReplayHively::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_module->ht_Name;
        for (uint8_t i = 1; i <= m_module->ht_InstrumentNr; i++)
        {
            metadata += "\n";
            metadata += m_module->ht_Instruments[i].ins_Name;
        }
        return metadata;
    }

    std::string ReplayHively::GetInfo() const
    {
        std::string info;

        char buf[32];
        sprintf(buf, "%d", m_module->ht_Channels);
        info += buf;
        info += " channels\n";

        if (m_mediaType.ext == eExtension::_ahx)
        {
            info += "AHX v";
            info += '1' + m_module->ht_Version;
        }
        else
        {
            info += "HVL 1.0-1.4";
        }
        info += "\nHivelyTracker 1.9";

        return info;
    }
}
// namespace rePlayer