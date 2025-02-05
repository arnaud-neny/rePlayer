#include "ReplayFutureComposer.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "libfc14audiodecoder/fc14audiodecoder.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::FutureComposer,
        .name = "Future Composer",
        .extensions = "smod;fc",
        .about = "Future Composer 1.0.3",
        .settings = "Future Composer 1.0.3",
        .init = ReplayFutureComposer::Init,
        .load = ReplayFutureComposer::Load,
        .displaySettings = ReplayFutureComposer::DisplaySettings,
        .editMetadata = ReplayFutureComposer::Settings::Edit
    };

    bool ReplayFutureComposer::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayFutureComposerStereoSeparation");
        window.RegisterSerializedData(ms_isNtsc, "ReplayFutureComposerNtsc");
        window.RegisterSerializedData(ms_surround, "ReplayFutureComposerSurround");

        return false;
    }

    Replay* ReplayFutureComposer::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024)
            return nullptr;
        auto decoder = fc14dec_new();
        fc14dec_mixer_init(decoder, kSampleRate, 16, 2, 0);
        auto data = stream->Read();
        if (fc14dec_detect(decoder, const_cast<uint8_t*>(data.Items()), static_cast<unsigned long int>(data.Size())) == 0)
        {
            fc14dec_delete(decoder);
            return nullptr;
        }

        if (fc14dec_init(decoder, const_cast<uint8_t*>(data.Items()), static_cast<unsigned long int>(data.Size())) == 0)
        {
            fc14dec_delete(decoder);
            return nullptr;
        }

        return new ReplayFutureComposer(decoder);
    }

    bool ReplayFutureComposer::DisplaySettings()
    {
        bool changed = false;
        const char* const clocks[] = { "PAL", "NTSC" };
        changed |= ImGui::Combo("Amiga Clock###FCAmigaClock", &ms_isNtsc, clocks, NumItemsOf(clocks));
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayFutureComposer::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("AmigaClock", GETSET(entry, overrideNtsc), GETSET(entry, isNtsc),
            ms_isNtsc, "Amiga Clock PAL", "Amiga Clock NTSC");
        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayFutureComposer::ms_stereoSeparation = 100;
    int32_t ReplayFutureComposer::ms_isNtsc = 0;
    int32_t ReplayFutureComposer::ms_surround = 1;

    ReplayFutureComposer::~ReplayFutureComposer()
    {
        fc14dec_delete(m_decoder);
    }

    ReplayFutureComposer::ReplayFutureComposer(void* decoder)
        : Replay(fc14dec_isFC14(decoder) == 0 ? eExtension::_smod : eExtension::_fc, eReplay::FutureComposer)
        , m_decoder(decoder)
        , m_surround(kSampleRate)
    {}

    uint32_t ReplayFutureComposer::Render(StereoSample* output, uint32_t numSamples)
    {
        auto samples = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;

        auto usedSize = fc14dec_buffer_fill(m_decoder, samples, static_cast<unsigned long>(numSamples * sizeof(int16_t) * 2));
        numSamples = usedSize / sizeof(int16_t) / 2;

        output->Convert(m_surround, samples, numSamples, m_stereoSeparation);

        return numSamples;
    }

    uint32_t ReplayFutureComposer::Seek(uint32_t timeInMs)
    {
        fc14dec_seek(m_decoder, timeInMs);
        m_surround.Reset();
        return timeInMs;
    }


    void ReplayFutureComposer::ResetPlayback()
    {
        fc14dec_restart(m_decoder);
        m_surround.Reset();
    }

    void ReplayFutureComposer::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        fc14dec_enableNtsc(m_decoder, (settings && settings->overrideNtsc) ? settings->isNtsc : ms_isNtsc);
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayFutureComposer::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
    }

    uint32_t ReplayFutureComposer::GetDurationMs() const
    {
        return fc14dec_duration(m_decoder);
    }

    uint32_t ReplayFutureComposer::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayFutureComposer::GetExtraInfo() const
    {
        std::string metadata;
        return metadata;
    }

    std::string ReplayFutureComposer::GetInfo() const
    {
        std::string info;

        info += "4 channels\n";
        info += fc14dec_format_name(m_decoder);
        info += "\nlibfc14audiodecoder-1.0.3";

        return info;
    }
}
// namespace rePlayer