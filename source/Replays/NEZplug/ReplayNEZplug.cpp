#include "ReplayNEZplug.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::NEZplug,
        .name = "NEZplug++",
        .extensions = "nsf;nsfe;hes;kss;gbr;gbs;ay;sgc;nsd",
        .about = "NEZplug++/libnezplug\n\nCopyright (c) 2019 MamiyaCopyright (c) 2020 John Regan",
        .settings = "NEZplug++",
        .init = ReplayNEZplug::Init,
        .load = ReplayNEZplug::Load,
        .displaySettings = ReplayNEZplug::DisplaySettings,
        .editMetadata = ReplayNEZplug::Settings::Edit
    };

    bool ReplayNEZplug::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_gain, "ReplayNEZplugGain");
        window.RegisterSerializedData(ms_filter, "ReplayNEZplugFilter");

        return false;
    }

    Replay* ReplayNEZplug::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();
        auto* nezplay = NEZNew();
        auto nezType = NEZLoad(nezplay, data.Items(), uint32_t(data.Size()));
        if (nezType != NEZ_TYPE_NONE)
        {
            eExtension nezTypeToExt[] = {
                eExtension::Unknown,
                eExtension::_kss,
                eExtension::_kss,
                eExtension::_hes,
                eExtension::_nsf,
                eExtension::_nsfe,
                eExtension::_ay,
                eExtension::_gbr,
                eExtension::_gbs,
                eExtension::_nsd,
                eExtension::_sgc
            };
            return new ReplayNEZplug(nezplay, nezTypeToExt[nezType], metadata);
        }
        NEZDelete(nezplay);

        return nullptr;
    }

    bool ReplayNEZplug::DisplaySettings()
    {
        bool changed = false;
        const char* const filter[] = { "None", "LowPass", "Weighted", "LowPass Weighted" };
        changed |= ImGui::Combo("Filter", &ms_filter, filter, _countof(filter));
        float gain = 1.0f + 7.0f * ms_gain / 255.0f;
        changed |= ImGui::SliderFloat("Gain", &gain, 1.0f, 8.0f, "x%1.3f", ImGuiSliderFlags_NoInput);
        ms_gain = int32_t(255.0f * (gain - 1.0f) / 7.0f);
        return changed;
    }

    void ReplayNEZplug::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(uint32_t));
            entry->GetDurations()[0] = 0;
        }

        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "Filter: None", "Filter: LowPass", "Filter: Weighted", "Filter: LowPass Weighted");
        SliderOverride("Gain", GETSET(entry, overrideGain), GetSet([entry]() { return 1.0f + 7.0f * entry->gain / 255.0f; }, [entry](auto v) { entry->gain = uint32_t(255.0f * (v - 1.0f) / 7.0f); }),
            1.0f + 7.0f * ms_gain / 255.0f, 1.0f, 8.0f, "Gain x %1.3f");
        Durations(context, entry->GetDurations(), entry->numSongsMinusOne + 1, "Subsong #%d Duration");

        context.metadata.Update(entry, false);
    }

    int32_t ReplayNEZplug::ms_gain = int32_t(255.0f * 2.0f / 7.0f);
    int32_t ReplayNEZplug::ms_filter = 0;

    ReplayNEZplug::~ReplayNEZplug()
    {
        NEZDelete(m_nezPlay);
    }

    ReplayNEZplug::ReplayNEZplug(NEZ_PLAY* nezPlay, eExtension extension, CommandBuffer metadata)
        : Replay(extension, eReplay::NEZplug)
        , m_nezPlay(nezPlay)
    {
        NEZSetFrequency(nezPlay, kSampleRate);
        NEZSetChannel(nezPlay, 2);
        NEZSetLength(nezPlay, 0);

        SetupMetadata(metadata);
    }

    uint32_t ReplayNEZplug::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (m_currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        NEZRender(m_nezPlay, output, numSamples);

        return numSamples;
    }

    uint32_t ReplayNEZplug::Seek(uint32_t timeInMs)
    {
        uint64_t seekPosition = timeInMs;
        seekPosition *= kSampleRate;
        seekPosition /= 1000;
        uint64_t currentPosition = m_currentPosition;
        if (seekPosition < currentPosition)
        {
            ResetPlayback();
            currentPosition = 0;
        }
        while (currentPosition != seekPosition)
        {
            uint32_t numSamples = uint32_t(Min(seekPosition - currentPosition, 0xffFFffFFull));
            NEZRender(m_nezPlay, nullptr, numSamples);
            currentPosition += numSamples;
        }
        m_currentPosition = seekPosition;
        return timeInMs;
    }

    void ReplayNEZplug::ResetPlayback()
    {
        NEZSetSongNo(m_nezPlay, m_subsongIndex + 1);
        NEZReset(m_nezPlay);

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
    }

    void ReplayNEZplug::ApplySettings(const CommandBuffer metadata)
    {
        auto* settings = metadata.Find<Settings>();
        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
        }

        NEZSetFilter(m_nezPlay, (settings && settings->overrideFilter) ? settings->filter : ms_filter);
        NEZGain(m_nezPlay, (settings && settings->overrideGain) ? settings->gain : ms_gain);
    }

    void ReplayNEZplug::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayNEZplug::GetDurationMs() const
    {
        return NEZGetTrackLength(m_nezPlay, m_subsongIndex + 1);
    }

    uint32_t ReplayNEZplug::GetNumSubsongs() const
    {
        return NEZGetSongMaxAbsolute(m_nezPlay);
    }

    std::string ReplayNEZplug::GetExtraInfo() const
    {
        std::string metadata;
        if (m_nezPlay->songinfodata.title)
            metadata = m_nezPlay->songinfodata.title;
        if (m_nezPlay->songinfodata.artist)
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_nezPlay->songinfodata.artist;
        }
        if (m_nezPlay->songinfodata.copyright)
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_nezPlay->songinfodata.copyright;
        }
        if (!metadata.empty())
            metadata += "\n\n";
        metadata = m_nezPlay->songinfodata.detail;
        return metadata;
    }

    std::string ReplayNEZplug::GetInfo() const
    {
        std::string info;
        info = NEZGetChannel(m_nezPlay) == 1 ? "1 channel\n" : "2 channels\n";
        if (m_mediaType.ext == eExtension::_kss)
            info.append(m_nezPlay->songinfodata.detail + 15, 4);
        else if (m_mediaType.ext == eExtension::_gbr || m_mediaType.ext == eExtension::_gbs)
            info += "Gameboy Sound System";
        else if (m_mediaType.ext == eExtension::_nsf || m_mediaType.ext == eExtension::_nsfe)
            info += "Nintendo Sound format";
        else if (m_mediaType.ext == eExtension::_hes)
            info += "Home Entertainment System PSG";
        else if (m_mediaType.ext == eExtension::_sgc)
        {
            const char* txt = m_nezPlay->songinfodata.detail + sizeof("Type          : SGC\r\nSystem        :");
            const char* end = txt;
            while (*end++ != '\r');
            info.append(txt, end);
        }
        else if (m_mediaType.ext == eExtension::_nsd)
            info += "NEZplug sound format";
        else if (m_mediaType.ext == eExtension::_ay)
            info += "ZXAYEMUL";
        info += "\n";
        info += "NEZplug++";
        return info;
    }

    void ReplayNEZplug::SetupMetadata(CommandBuffer metadata)
    {
        auto numSongsMinusOne = NEZGetSongMaxAbsolute(m_nezPlay) - 1;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongsMinusOne == numSongsMinusOne)
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0; i <= numSongsMinusOne; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + (numSongsMinusOne + 1) * sizeof(int32_t));
            settings->value = value;
            settings->numSongsMinusOne = numSongsMinusOne;
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= numSongsMinusOne; i++)
            {
                durations[i] = m_durations[i] = NEZGetTrackLength(m_nezPlay, i + 1);
            }
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
    }
}
// namespace rePlayer