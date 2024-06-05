#include "ReplaySidPlay.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/File.h>
#include <ReplayDll.h>

#include "libsidplayfp/config.h"
#include "libsidplayfp/builders/residfp-builder/residfp.h"
#include "libsidplayfp/sidplayfp/SidInfo.h"
#include "libsidplayfp/sidplayfp/SidTune.h"
#include "libsidplayfp/sidplayfp/SidTuneInfo.h"
#include "libsidplayfp/sidplayfp/sidplayfp.h"
#include "libsidplayfp/utils/SidDatabase.h"

#include <filesystem>

namespace libsidplayfp
{
    class loadError
    {
    private:
        const char* m_msg;
    public:
        loadError(const char* msg) : m_msg(msg) {}
        const char* message() const { return m_msg; }
    };
}
// namespace libsidplayfp

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SidPlay,
        .name = "SidPlay",
        .extensions = "psid;rsid;sid;mus",
        .about = PACKAGE_STRING "\nCopyright (c) 2000 Simon White\nCopyright (c) 2007-2010 Antti Lankila\nCopyright (c) 2010-2023 Leandro Nini",
        .settings = "SidPlay " PACKAGE_VERSION,
        .init = ReplaySidPlay::Init,
        .release = ReplaySidPlay::Release,
        .load = ReplaySidPlay::Load,
        .displaySettings = ReplaySidPlay::DisplaySettings,
        .editMetadata = ReplaySidPlay::Settings::Edit
    };

    bool ReplaySidPlay::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_isFilterEnabled, "ReplaySidPlayFilter");
        window.RegisterSerializedData(ms_filter6581, "ReplaySidPlayFilter6581");
        window.RegisterSerializedData(ms_filter8580, "ReplaySidPlayFilter8580");
        window.RegisterSerializedData(ms_isSidModel8580, "ReplaySidPlayModel8580");
        window.RegisterSerializedData(ms_isNtsc, "ReplaySidPlayNtsc");
        window.RegisterSerializedData(ms_isResampling, "ReplaySidPlayResampling");
        window.RegisterSerializedData(ms_surround, "ReplaySidPlaySurround");
        window.RegisterSerializedData(ms_powerOnDelay, "ReplaySidPlayPowerOnDelay");
        window.RegisterSerializedData(ms_combinedWaveforms, "ReplaySidPlayCombinedWaveforms");

        return false;
    }

    Replay* ReplaySidPlay::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;
        auto data = stream->Read();

        struct Loader
        {
            static void cb(const char* fileName, std::vector<uint8_t>& bufferRef, void* loaderData)
            {
                auto loader = reinterpret_cast<Loader*>(loaderData);
                for (auto& stream : loader->streams)
                {
                    if (_stricmp(stream->GetName().c_str(), fileName) == 0)
                    {
                        auto data = stream->Read();
                        bufferRef.assign(data.Items(), data.Items(data.NumItems()));
                        return;
                    }
                }
                if (auto strStream = loader->streams[0]->Open(fileName))
                {
                    auto data = strStream->Read();
                    bufferRef.assign(data.Items(), data.Items(data.NumItems()));
                    loader->streams.Add(strStream);
                    return;
                }
                throw libsidplayfp::loadError("Can't open file");
            }
            Array<SmartPtr<io::Stream>> streams;
        } loader;
        loader.streams.Add(stream);
        SidTune* sidTune = new SidTune(loader.cb, &loader, stream->GetName().c_str());
        if (!sidTune->getStatus())
        {
            //printf("%s", sidTune->statusString());
            delete sidTune;
            return nullptr;
        }
        if (sidTune->getInfo()->sidChips() == 1)
            return new ReplaySidPlay(sidTune, new SidTune(data.Items(), static_cast<uint32_t>(data.Size())),metadata);
        return new ReplaySidPlay(sidTune, nullptr, metadata);
    }

    bool ReplaySidPlay::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Power On Delay", &ms_powerOnDelay, 0, SidConfig::MAX_POWER_ON_DELAY, "%d cycles", ImGuiSliderFlags_AlwaysClamp);
        {
            const char* const sidModels[] = { "6581", "8580" };
            int index = ms_isSidModel8580 ? 1 : 0;
            changed |= ImGui::Combo("SID Default Model", &index, sidModels, _countof(sidModels));
            ms_isSidModel8580 = index == 1;
        }
        {
            const char* const clocks[] = { "PAL", "NTSC" };
            auto index = ms_isNtsc ? 1 : 0;
            changed |= ImGui::Combo("Default Clock###SidClock", &index, clocks, _countof(clocks));
            ms_isNtsc = index == 1;
        }
        {
            const char* const samplings[] = { "Interpolate", "Resample" };
            auto index = ms_isResampling ? 1 : 0;
            changed |= ImGui::Combo("Sampling###SidSampling", &index, samplings, _countof(samplings));
            ms_isResampling = index == 1;
        }
        {
            const char* const samplings[] = { "Off", "On" };
            auto index = ms_isFilterEnabled ? 1 : 0;
            changed |= ImGui::Combo("Filter###SidFilter", &index, samplings, _countof(samplings));
            ms_isFilterEnabled = index != 0;
        }
        changed |= ImGui::SliderInt("Filter 6581###SidFilter6581", &ms_filter6581, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::SliderInt("Filter 8580###SidFilter8580", &ms_filter8580, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
        {
            const char* const combinedWaveforms[] = { "Average", "Weak", "Strong" };
            changed |= ImGui::Combo("Combined Waveforms###SidCW", &ms_combinedWaveforms, combinedWaveforms, _countof(combinedWaveforms));
        }
        {
            const char* const surround[] = { "Default", "Surround" };
            changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        }
        return changed;
    }

    void ReplaySidPlay::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>();
        }

        SliderOverride("PowerOnDelay", GETSET(entry, overridePowerOnDelay), GETSET(entry, powerOnDelay),
            ms_powerOnDelay, 0, SidConfig::MAX_POWER_ON_DELAY, "Power On Delay: %d cycles");
        ComboOverride("SidFilter", GETSET(entry, overrideEnableFilter), GETSET(entry, filterEnabled),
            ms_isFilterEnabled, "Filter: Disable", "Filter: Enable");
        SliderOverride("Filter6581", GETSET(entry, overrideFilter6581), GETSET(entry, filter6581),
            ms_filter6581, 0, 100, "Filter 6581: %d%%");
        SliderOverride("Filter8580", GETSET(entry, overrideFilter8580), GETSET(entry, filter8580),
            ms_filter8580, 0, 100, "Filter 8580: %d%%");
        ComboOverride("SidModel", GETSET(entry, overrideSidModel), GETSET(entry, sidModel),
            ms_isSidModel8580, "Sid Model: 6581", "Sid Model: 8580");
        ComboOverride("SidClock", GETSET(entry, overrideClock), GETSET(entry, clock),
            ms_isNtsc, "Clock Speed: PAL", "Clock Speed: NTSC");
        ComboOverride("SidSampling", GETSET(entry, overrideResampling), GETSET(entry, resampling),
            ms_isResampling, "Sampling: Interpolate", "Sampling: Resample");
        ComboOverride("SidCW", GETSET(entry, overrideCombinedWaveforms), GETSET(entry, combinedWaveforms),
            ms_combinedWaveforms, "Combined Waveforms: Average", "Combined Waveforms: Weak", "Combined Waveforms: Strong");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Default", "Output: Surround");
        Durations(context, entry->GetDurations(), entry->numSongs, "Subsong #%d Duration");
    }

    uint8_t ReplaySidPlay::ms_c64RomKernal[] = {
        #include "ReplaySidPlayKernal.inl"
    };
    uint8_t ReplaySidPlay::ms_c64RomBasic[] = {
        #include "ReplaySidPlayBasic.inl"
    };

    SidDatabase* ReplaySidPlay::ms_sidDatabase = nullptr;

    bool ReplaySidPlay::ms_isFilterEnabled = true;
    int32_t ReplaySidPlay::ms_filter6581 = 50;
    int32_t ReplaySidPlay::ms_filter8580 = 25;
    bool ReplaySidPlay::ms_isSidModel8580 = false;
    bool ReplaySidPlay::ms_isNtsc = false;
    bool ReplaySidPlay::ms_isResampling = false;
    int32_t ReplaySidPlay::ms_surround = 1;
    int32_t ReplaySidPlay::ms_powerOnDelay = 32;
    int32_t ReplaySidPlay::ms_combinedWaveforms = SidConfig::AVERAGE;

    void ReplaySidPlay::Release()
    {
        if (ms_sidDatabase)
        {
            ms_sidDatabase->close();
            delete ms_sidDatabase;
        }
    }

    ReplaySidPlay::~ReplaySidPlay()
    {
        delete[] m_durations;
        for (uint32_t i = 0; i < 2; i++)
        {
            delete m_sidplayfp[i];
            delete m_sidTune[i];
            delete m_residfpBuilder[i];
        }
    }

    ReplaySidPlay::ReplaySidPlay(SidTune* sidTune1, SidTune* sidTune2, CommandBuffer metadata)
        : Replay(GetExtension(sidTune1), eReplay::SidPlay)
        , m_sidTune{ sidTune1, sidTune2 }
        , m_surround(kSampleRate)
    {
        auto numSongs = sidTune1->getInfo()->songs();
        m_durations = new uint32_t[numSongs];
        for (uint32_t i = 0; i < numSongs; i++)
            m_durations[i] = kDefaultSongDuration;

        uint32_t numEmu = sidTune1->getInfo()->sidChips() == 1 ? 2 : 1;
        for (uint32_t i = 0; i < numEmu; i++)
        {
            m_sidTune[i]->selectSong(m_subsongIndex + 1);
            m_sidplayfp[i] = new sidplayfp();
            m_sidplayfp[i]->setRoms(ms_c64RomKernal, ms_c64RomBasic, nullptr);

            m_residfpBuilder[i] = new ReSIDfpBuilder("rePlayer");

            // Create SID emulators
            m_residfpBuilder[i]->create(m_sidTune[i]->getInfo()->sidChips());
            if (!m_residfpBuilder[i]->getStatus())
            {
                //printf("%s", m_residfpBuilder->error());
            }

            // Configure the engine
            SidConfig cfg;
            cfg.frequency = kSampleRate;
            cfg.samplingMethod = ms_isResampling ? SidConfig::RESAMPLE_INTERPOLATE : SidConfig::INTERPOLATE;
            cfg.fastSampling = false;
            cfg.playback = numEmu == 2 ? SidConfig::MONO : SidConfig::STEREO;
            cfg.sidEmulation = m_residfpBuilder[i];
            cfg.defaultSidModel = ms_isSidModel8580 ? SidConfig::MOS8580 : SidConfig::MOS6581;
            cfg.defaultC64Model = ms_isNtsc ? SidConfig::NTSC : SidConfig::PAL;
            cfg.powerOnDelay = uint16_t(ms_powerOnDelay);

            if (!m_sidplayfp[i]->config(cfg))
            {
                //printf("%s", m_sidplayfp->error());
            }

            if (!m_sidplayfp[i]->load(m_sidTune[i]))
            {
                //printf("%s", m_sidplayfp->error());
            }
        }

        SetupMetadata(metadata);
    }

    uint32_t ReplaySidPlay::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if ((currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        if (m_sidplayfp[1] != nullptr)
        {
            auto numRemainingSamples = numSamples;
            auto numCachedSamples = m_numSamples;
            auto* samples = output;
            while (numRemainingSamples)
            {
                if (numCachedSamples == 0)
                {
                    auto numSamplesLeft = m_sidplayfp[0]->play(m_samples, kNumSamples);
                    auto numSamplesRight = m_sidplayfp[1]->play(m_samples + kNumSamples, kNumSamples);
                    if (numSamplesLeft != numSamplesRight || numSamplesLeft == 0 || !m_sidplayfp[0]->isPlaying() || !m_sidplayfp[1]->isPlaying()) // todo error reporting?
                        return 0;
                    numCachedSamples = kNumSamples;
                }
                else
                {
                    auto samplesLeft = m_samples + kNumSamples - numCachedSamples;
                    auto samplesRight = samplesLeft + (m_surround.IsEnabled() ? kNumSamples : 0);
                    auto numSamplesToCopy = Min(numRemainingSamples, numCachedSamples);
                    samples = samples->Convert(m_surround, samplesLeft, samplesRight, numSamplesToCopy, 100, m_surround.IsEnabled() ? 3.0f : 2.0f);
                    numRemainingSamples -= numSamplesToCopy;
                    numCachedSamples -= numSamplesToCopy;
                }
            }
            m_numSamples = numCachedSamples;
        }
        else
        {
            auto numRemainingSamples = numSamples;
            auto numCachedSamples = m_numSamples;
            auto* samples = output;
            while (numRemainingSamples)
            {
                if (numCachedSamples == 0)
                {
                    auto numSamplesStereo = m_sidplayfp[0]->play(m_samples, kNumSamples * 2);
                    if (numSamplesStereo == 0 || !m_sidplayfp[0]->isPlaying()) // todo error reporting?
                        return 0;
                    numCachedSamples = kNumSamples;
                }
                else
                {
                    auto samplesStereo = m_samples + (kNumSamples - numCachedSamples) * 2;
                    auto numSamplesToCopy = Min(numRemainingSamples, numCachedSamples);
                    samples = samples->Convert(m_surround, samplesStereo, numSamplesToCopy, 100, m_surround.IsEnabled() ? 2.6f : 2.0f);
                    numRemainingSamples -= numSamplesToCopy;
                    numCachedSamples -= numSamplesToCopy;
                }
            }
            m_numSamples = numCachedSamples;
        }
        if (auto numBootSamples = m_numBootSamples)
        {
            //remove the boot sound glitch...
            auto maxBootSamples = GetMaxBootingSamples();
            auto offset = maxBootSamples - numBootSamples;
            numBootSamples = Min(numBootSamples, numSamples);
            for (uint32_t i = 0; i < numBootSamples; i++)
            {
                auto p = i + offset;
                output->left *= output->left * (p * p) / (maxBootSamples * maxBootSamples);
                output->right *= output->right * (p * p) / (maxBootSamples * maxBootSamples);
                output++;
            }
            m_numBootSamples -= numBootSamples;
        }

        return numSamples;
    }

    void ReplaySidPlay::ResetPlayback()
    {
        m_surround.Reset();
        m_sidplayfp[0]->stop();
        if (m_sidplayfp[1])
            m_sidplayfp[1]->stop();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
        m_numBootSamples = GetMaxBootingSamples();
        m_sidplayfp[0]->play(nullptr, 0);
        if (m_sidplayfp[1])
            m_sidplayfp[1]->play(nullptr, 0);
        m_numSamples = 0;
    }

    void ReplaySidPlay::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i < settings->numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }

        uint16_t powerOnDelay = uint16_t(settings && settings->overridePowerOnDelay ? settings->powerOnDelay : ms_powerOnDelay);
        bool isSidModelForced = settings && settings->overrideSidModel;
        bool isSidModel8580 = isSidModelForced ? settings->sidModel : ms_isSidModel8580;
        bool isClockForced = settings && settings->overrideClock;
        bool isNtsc = isClockForced ? settings->clock : ms_isNtsc;
        bool isResampling = settings && settings->overrideResampling ? settings->resampling : ms_isResampling;
        for (uint32_t sidIndex = 0; sidIndex < 2 && m_sidTune[sidIndex] != nullptr; sidIndex++)
        {
            m_residfpBuilder[sidIndex]->filter((settings && settings->overrideEnableFilter) ? settings->filterEnabled : ms_isFilterEnabled);
            m_residfpBuilder[sidIndex]->filter6581Curve(((settings && settings->overrideFilter6581) ? settings->filter6581 : ms_filter6581) / 100.0f);
            m_residfpBuilder[sidIndex]->filter8580Curve(((settings && settings->overrideFilter8580) ? settings->filter8580 : ms_filter8580) / 100.0f);
            m_residfpBuilder[sidIndex]->combinedWaveformsStrength(SidConfig::sid_cw_t((settings && settings->overrideCombinedWaveforms) ? settings->combinedWaveforms : ms_combinedWaveforms));

            if (m_currentPosition == 0 && (m_isSidModelForced != isSidModelForced || m_isSidModel8580 != isSidModel8580 || m_isClockForced != isClockForced || m_isNtsc != isNtsc || m_powerOnDelay != powerOnDelay))
            {
                SidConfig cfg = m_sidplayfp[sidIndex]->config();
                cfg.samplingMethod = isResampling ? SidConfig::RESAMPLE_INTERPOLATE : SidConfig::INTERPOLATE;
                cfg.defaultSidModel = isSidModel8580 ? SidConfig::MOS8580 : SidConfig::MOS6581;
                cfg.defaultC64Model = isNtsc ? SidConfig::NTSC : SidConfig::PAL;
                cfg.forceSidModel = isSidModelForced;
                cfg.forceC64Model = isClockForced;
                cfg.powerOnDelay = powerOnDelay;
                if (!m_sidplayfp[sidIndex]->config(cfg))
                {
                    //printf("%s", m_sidplayfp[sidIndex]->error());
                }
                m_sidplayfp[sidIndex]->play(nullptr, 0);
            }
        }
        m_isSidModelForced = isSidModelForced;
        m_isSidModel8580 = isSidModel8580;
        m_isClockForced = isClockForced;
        m_isNtsc = isNtsc;
        m_isResampling = isResampling;
        m_powerOnDelay = powerOnDelay;

        bool isSurroundEnable = (settings && settings->overrideSurround) ? settings->surround : ms_surround;
        m_surround.Enable(isSurroundEnable);
        if (m_sidTune[1] != nullptr)
        {
            m_sidplayfp[0]->mute(0, 1, isSurroundEnable);
            m_sidplayfp[1]->mute(0, 0, isSurroundEnable);
            m_sidplayfp[1]->mute(0, 2, isSurroundEnable);
        }
    }

    void ReplaySidPlay::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
        for (uint32_t i = 0; i < 2; i++)
        {
            if (m_sidTune[i])
            {
                m_sidTune[i]->selectSong(subsongIndex + 1);
                m_sidplayfp[i]->load(m_sidTune[i]);
            }
        }
    }

    eExtension ReplaySidPlay::GetExtension(SidTune* sidTune)
    {
        auto format = sidTune->getInfo()->formatString();
        if (strstr(format, "PSID"))
            return eExtension::_sid;
        if (strstr(format, "RSID"))
            return eExtension::_sid;
        if (strstr(format, "MUS"))
            return eExtension::_mus;
        assert(0);
        return eExtension::Unknown;
    }

    uint32_t ReplaySidPlay::GetDurationMs() const
    {
        return m_durations[m_subsongIndex];
    }

    uint32_t ReplaySidPlay::GetNumSubsongs() const
    {
        return m_sidTune[0]->getInfo()->songs();
    }

    std::string ReplaySidPlay::GetExtraInfo() const
    {
        std::string metadata;
        auto info = m_sidTune[0]->getInfo();
        for (uint32_t i = 0, e = info->numberOfInfoStrings(); i < e; i++)
        {
            if (i != 0)
                metadata += "\n";
            metadata += info->infoString(i);
        }
        for (uint32_t i = 0, e = info->numberOfCommentStrings(); i < e; i++)
        {
            if (i != 0 || !metadata.empty())
                metadata += "\n";
            metadata += info->commentString(i);
        }
        return metadata;
    }

    std::string ReplaySidPlay::GetInfo() const
    {
        std::string info;
        char txt[16];
        sprintf(txt, "%d channels", 3 * m_sidTune[0]->getInfo()->sidChips());
        info = txt;
        for (int i = 0; i < m_sidTune[0]->getInfo()->sidChips(); i++)
        {
            info += i == 0 ? " " : "/";
            if (m_isSidModelForced)
                info += m_isSidModel8580 ? "8580" : "6581";
            else
                info += m_sidTune[0]->getInfo()->sidModel(i) == SidTuneInfo::SIDMODEL_6581 ? "6581" : "8580";
        }
        info += "\n";
        info += m_sidTune[0]->getInfo()->formatString();
        info += "\n" PACKAGE_STRING;
        return info;
    }

    inline uint32_t ReplaySidPlay::GetMaxBootingSamples() const
    {
        return kSampleRate / 16;
    }

    void ReplaySidPlay::SetupMetadata(CommandBuffer metadata)
    {
        auto numSongs = m_sidTune[0]->getInfo()->songs();
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongs == numSongs)
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0; i < numSongs; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
        else
        {
            uint32_t value[2] = { settings ? settings->value[0] : 0, settings ? settings->value[1] : 0 };
            settings = metadata.Create<Settings>(sizeof(Settings) + numSongs * sizeof(uint32_t));
            settings->value[0] = value[0];
            settings->value[1] = value[1];
            settings->numSongs = numSongs;

            auto sidDatabase = ms_sidDatabase;
            if (sidDatabase == nullptr)
            {
                ms_sidDatabase = sidDatabase = new SidDatabase();
                auto buffer = g_replayPlugin.download("https://hvsc.de/download/C64Music/DOCUMENTS/Songlengths.md5");
                if (buffer.IsEmpty())
                    buffer = g_replayPlugin.download("https://hvsc.etv.cx/C64Music/DOCUMENTS/Songlengths.md5");
                if (buffer.IsNotEmpty())
                {
                    std::filesystem::path path = std::filesystem::temp_directory_path();
                    path.replace_filename("siddb.txt");
                    {
                        auto file = io::File::OpenForWrite(path.string().c_str());
                        file.Write(buffer.Items(), buffer.Size());
                    }
                    sidDatabase->open(path.string().c_str());
                    io::File::Delete(path.string().c_str());
                }
            }

            auto* durations = settings->GetDurations();

            char md5[SidTune::MD5_LENGTH + 1];
            m_sidTune[0]->createMD5New(md5);

            for (uint32_t i = 0; i < numSongs; i++)
            {
                auto duration = sidDatabase->lengthMs(md5, i + 1);
                if (duration == -1)
                    duration = kDefaultSongDuration;
                durations[i] = duration;
                m_durations[i] = durations[i];
            }
        }
    }
}
// namespace rePlayer