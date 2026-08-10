// - builder/residfp-builder/residfp/FilterModelConfig6581.cpp
// - builder/residfp-builder/residfp/FilterModelConfig8580.cpp
// - sidplayfp/SidTune.cpp
// - sidplayfp/SidTune.h
// - sidplayfp/SidTuneBase.h
#include "ReplaySidPlay.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <IO/File.h>
#include <ReplayDll.h>

#include "libsidplayfp/config.h"
#include "libsidplayfp/builders/residfp-builder/residfp.h"
#include "libsidplayfp/builders/sidlite-builder/sidlite.h"
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

extern "C"
{
    extern const char* residfp_version_string;
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SidPlay,
        .name = "SidPlay",
        .extensions = "psid;rsid;sid;mus",
        .about = PACKAGE_STRING "\nCopyright (c) 2000 Simon White\nCopyright (c) 2007-2010 Antti Lankila\nCopyright (c) 2010-2026 Leandro Nini",
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

        window.RegisterSerializedData(ms_isFastSidEnabled, "ReplaySidPlayFastSid");
        window.RegisterSerializedData(ms_isFilterEnabled, "ReplaySidPlayFilter");
        window.RegisterSerializedData(ms_filter6581, "ReplaySidPlayFilter6581");
        window.RegisterSerializedData(ms_filter8580, "ReplaySidPlayFilter8580");
        window.RegisterSerializedData(ms_isSidModel8580, "ReplaySidPlayModel8580");
        window.RegisterSerializedData(ms_isNtsc, "ReplaySidPlayNtsc");
        window.RegisterSerializedData(ms_isResampling, "ReplaySidPlayResampling");
        window.RegisterSerializedData(ms_surround, "ReplaySidPlaySurround");
        window.RegisterSerializedData(ms_powerOnDelay, "ReplaySidPlayPowerOnDelay");
        window.RegisterSerializedData(ms_combinedWaveforms, "ReplaySidPlayCombinedWaveforms");
        window.RegisterSerializedData(ms_isOld6581capsEnable, "ReplaySidPlayOld6581caps");
        window.RegisterSerializedData(ms_DACLeakage, "ReplaySidPlayDACLeakage");
        window.RegisterSerializedData(ms_6581WaveOffset, "ReplaySidPlay6581WaveOffset");
        window.RegisterSerializedData(ms_DCBlockerResistance, "ReplaySidPlayDCBlockerResistance");

        return false;
    }

    Replay* ReplaySidPlay::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto streamSize = stream->GetSize();
        if (streamSize > 1024 * 1024 * 128 || streamSize == 0)
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
        return new ReplaySidPlay(sidTune, metadata);
    }

    bool ReplaySidPlay::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Power On Delay", &ms_powerOnDelay, 0, SidConfig::MAX_POWER_ON_DELAY, "%d cycles", ImGuiSliderFlags_AlwaysClamp);
        {
            const char* const sidModels[] = { "6581", "8580" };
            int index = ms_isSidModel8580 ? 1 : 0;
            changed |= ImGui::Combo("SID Default Model", &index, sidModels, NumItemsOf(sidModels));
            ms_isSidModel8580 = index == 1;
        }
        {
            const char* const clocks[] = { "PAL", "NTSC" };
            auto index = ms_isNtsc ? 1 : 0;
            changed |= ImGui::Combo("Default Clock###SidClock", &index, clocks, NumItemsOf(clocks));
            ms_isNtsc = index == 1;
        }
        {
            const char* const samplings[] = { "Interpolate", "Resample" };
            auto index = ms_isResampling ? 1 : 0;
            changed |= ImGui::Combo("Sampling###SidSampling", &index, samplings, NumItemsOf(samplings));
            ms_isResampling = index == 1;
        }
        {
            const char* const samplings[] = { "Off", "On" };
            auto index = ms_isFilterEnabled ? 1 : 0;
            changed |= ImGui::Combo("Filter###SidFilter", &index, samplings, NumItemsOf(samplings));
            ms_isFilterEnabled = index != 0;
        }
        {
            const char* const builder[] = { "ReSIDfp", "SIDLite" };
            auto index = ms_isFastSidEnabled ? 1 : 0;
            changed |= ImGui::Combo("Builder###FastSid", &index, builder, NumItemsOf(builder));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::Tooltip("Need song reload to be effective");
            ms_isFastSidEnabled = index != 0;
        }
        if (!ms_isFastSidEnabled)
        {
            changed |= ImGui::SliderInt("Filter 6581###SidFilter6581", &ms_filter6581, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Filter 8580###SidFilter8580", &ms_filter8580, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
            {
                const char* const combinedWaveforms[] = { "Average", "Weak", "Strong" };
                changed |= ImGui::Combo("Combined Waveforms###SidCW", &ms_combinedWaveforms, combinedWaveforms, NumItemsOf(combinedWaveforms));
            }
            {
                const char* const old6581caps[] = { "Off", "On" };
                auto index = ms_isOld6581capsEnable ? 1 : 0;
                changed |= ImGui::Combo("Old 6581 caps###Old6581caps", &index, old6581caps, NumItemsOf(old6581caps));
                ms_isOld6581capsEnable = index != 0;
            }
            auto f = float(double(ms_DACLeakage) / INT_MAX);
            changed |= ImGui::SliderFloat("DAC leakage level###DACLeakage", &f, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
            ms_DACLeakage = int32_t(double(f) * INT_MAX);
            f = float(double(ms_6581WaveOffset) / INT_MAX);
            changed |= ImGui::SliderFloat("6581 wave offset###6581WaveOffset", &f, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
            ms_6581WaveOffset = int32_t(double(f) * INT_MAX);
            f = float(double(ms_DCBlockerResistance) / INT_MAX);
            changed |= ImGui::SliderFloat("DC-Blocker resistance###DCBlockerResistance", &f, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
            ms_DCBlockerResistance = int32_t(double(f) * INT_MAX);
        }
        {
            const char* const surround[] = { "Default", "Surround" };
            changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        }
        return changed;
    }

    void ReplaySidPlay::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* oldEntry = context.metadata.Find<Settings>();
        auto* entry = new (_alloca(sizeof(Settings) + (oldEntry ? oldEntry->numSongs : 0) * sizeof(LoopInfo) + 3 * sizeof(int32_t))) Settings();
        if (oldEntry)
        {
            memcpy(entry, oldEntry, sizeof(Settings) + (oldEntry ? oldEntry->numSongs : 0) * sizeof(LoopInfo));
            int ofs = 0; // extra params are stacked after the loops if enabled
            pCast<int32_t>(entry->loops + entry->numSongs)[0] = oldEntry->overrideDACLeakage ? pcCast<int32_t>(oldEntry->loops + entry->numSongs)[ofs++] : ms_DACLeakage;
            pCast<int32_t>(entry->loops + entry->numSongs)[1] = oldEntry->override6581WaveOffset ? pcCast<int32_t>(oldEntry->loops + entry->numSongs)[ofs++] : ms_6581WaveOffset;
            pCast<int32_t>(entry->loops + entry->numSongs)[2] = oldEntry->overrideDCBlockerResistance ? pcCast<int32_t>(oldEntry->loops + entry->numSongs)[ofs++] : ms_DCBlockerResistance;
            entry->numEntries += uint16_t(3 - ofs);
        }
        else
            entry->numEntries += 3;

        SliderOverride("PowerOnDelay", GETSET(entry, overridePowerOnDelay), GETSET(entry, powerOnDelay),
            ms_powerOnDelay, 0, SidConfig::MAX_POWER_ON_DELAY, "Power On Delay: %d cycles");
        ComboOverride("SidFilter", GETSET(entry, overrideEnableFilter), GETSET(entry, filterEnabled),
            ms_isFilterEnabled, "Filter: Disable", "Filter: Enable");
        ComboOverride("SidModel", GETSET(entry, overrideSidModel), GETSET(entry, sidModel),
            ms_isSidModel8580, "Sid Model: 6581", "Sid Model: 8580");
        ComboOverride("SidClock", GETSET(entry, overrideClock), GETSET(entry, clock),
            ms_isNtsc, "Clock Speed: PAL", "Clock Speed: NTSC");
        ComboOverride("SidSampling", GETSET(entry, overrideResampling), GETSET(entry, resampling),
            ms_isResampling, "Sampling: Interpolate", "Sampling: Resample");
        ComboOverride("Builder", GETSET(entry, overrideEnableFastSid), GETSET(entry, fastSidEnabled),
            ms_isFastSidEnabled, "Builder: ReSIDfp", "Builder: SIDLite");
        SliderOverride("Filter6581", GETSET(entry, overrideFilter6581), GETSET(entry, filter6581),
            ms_filter6581, 0, 100, "Filter 6581: %d%%");
        SliderOverride("Filter8580", GETSET(entry, overrideFilter8580), GETSET(entry, filter8580),
            ms_filter8580, 0, 100, "Filter 8580: %d%%");
        ComboOverride("SidCW", GETSET(entry, overrideCombinedWaveforms), GETSET(entry, combinedWaveforms),
            ms_combinedWaveforms, "Combined Waveforms: Average", "Combined Waveforms: Weak", "Combined Waveforms: Strong");
        ComboOverride("Old6581caps", GETSET(entry, overrideOld6581caps), GETSET(entry, old6581capsEnabled),
            ms_isOld6581capsEnable, "Old 6581 caps: Disable", "Old 6581 caps: Enable");
        auto f = float(double(pcCast<int32_t>(entry->loops + entry->numSongs)[0]) / INT_MAX);
        auto df = float(double(ms_DACLeakage) / INT_MAX);
        SliderOverride("DACLeakage", GETSET(entry, overrideDACLeakage)
            , GetSet([&]() { return f; }, [&](auto v) { f = v; })
            , df, 0.0f, 1.0f, "DAC leakage level: %.5f");
        pCast<int32_t>(entry->loops + entry->numSongs)[0] = int32_t(double(f) * INT_MAX);
        f = float(double(pcCast<int32_t>(entry->loops + entry->numSongs)[1]) / INT_MAX);
        df = float(double(ms_6581WaveOffset) / INT_MAX);
        SliderOverride("6581WaveOffset", GETSET(entry, override6581WaveOffset)
            , GetSet([&]() { return f; }, [&](auto v) { f = v; })
            , df, 0.0f, 1.0f, "6581 wave offset: %.5f");
        pCast<int32_t>(entry->loops + entry->numSongs)[1] = int32_t(double(f) * INT_MAX);
        f = float(double(pcCast<int32_t>(entry->loops + entry->numSongs)[2]) / INT_MAX);
        df = float(double(ms_DCBlockerResistance) / INT_MAX);
        SliderOverride("DCBlockerResistance", GETSET(entry, overrideDCBlockerResistance)
            , GetSet([&]() { return f; }, [&](auto v) { f = v; })
            , df, 0.0f, 1.0f, "DC-Blocker resistance: %.5f");
        pCast<int32_t>(entry->loops + entry->numSongs)[2] = int32_t(double(f) * INT_MAX);
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Default", "Output: Surround");
        Loops(context, entry->loops, entry->numSongs);

        int ofs = 0; // extra params are stacked after the loops if enabled
        if (entry->overrideDACLeakage)
            ofs++;
        if (entry->override6581WaveOffset)
            pCast<int32_t>(entry->loops + entry->numSongs)[ofs++] = pcCast<int32_t>(entry->loops + entry->numSongs)[1];
        if (entry->overrideDCBlockerResistance)
            pCast<int32_t>(entry->loops + entry->numSongs)[ofs++] = pcCast<int32_t>(entry->loops + entry->numSongs)[2];
        entry->numEntries -= uint16_t(3 - ofs);
        context.metadata.Update(entry, false);
    }

    uint8_t ReplaySidPlay::ms_c64RomKernal[] = {
        #include "ReplaySidPlayKernal.inl"
    };
    uint8_t ReplaySidPlay::ms_c64RomBasic[] = {
        #include "ReplaySidPlayBasic.inl"
    };

    SidDatabase* ReplaySidPlay::ms_sidDatabase = nullptr;

    bool ReplaySidPlay::ms_isFastSidEnabled = false;
    bool ReplaySidPlay::ms_isFilterEnabled = true;
    int32_t ReplaySidPlay::ms_filter6581 = 50;
    int32_t ReplaySidPlay::ms_filter8580 = 50;
    bool ReplaySidPlay::ms_isSidModel8580 = false;
    bool ReplaySidPlay::ms_isNtsc = false;
    bool ReplaySidPlay::ms_isResampling = false;
    int32_t ReplaySidPlay::ms_surround = 1;
    int32_t ReplaySidPlay::ms_powerOnDelay = 4096;
    int32_t ReplaySidPlay::ms_combinedWaveforms = SidConfig::AVERAGE;
    bool ReplaySidPlay::ms_isOld6581capsEnable = false;
    int32_t ReplaySidPlay::ms_DACLeakage = INT_MAX;
    int32_t ReplaySidPlay::ms_6581WaveOffset = INT_MAX;
    int32_t ReplaySidPlay::ms_DCBlockerResistance = INT_MAX;

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
        delete[] m_loops;
        delete m_sidplayfp;
        delete m_sidTune;
        delete m_sidBuilder;
    }

    ReplaySidPlay::ReplaySidPlay(SidTune* sidTune, CommandBuffer metadata)
        : Replay(GetExtension(sidTune), eReplay::SidPlay)
        , m_sidTune(sidTune)
        , m_surround(kSampleRate)
    {
        auto numSongs = sidTune->getInfo()->songs();
        m_loops = new LoopInfo[numSongs];
        for (uint32_t i = 0; i < numSongs; i++)
            m_loops[i] = { 0, kDefaultSongDuration };

        auto settings = metadata.Find<Settings>();
        m_isFastSid = settings && settings->overrideEnableFastSid ? settings->fastSidEnabled : ms_isFastSidEnabled;

        m_sidTune->selectSong(m_subsongIndex + 1);
        m_sidplayfp = new sidplayfp();
        m_sidplayfp->setRoms(ms_c64RomKernal, ms_c64RomBasic, nullptr);

        if (m_isFastSid)
            m_sidBuilder = new SIDLiteBuilder("rePlayer");
        else
            m_sidBuilder = new ReSIDfpBuilder("rePlayer");

        // Configure the engine
        SidConfig cfg;
        cfg.frequency = kSampleRate;
        cfg.samplingMethod = ms_isResampling ? SidConfig::RESAMPLE_INTERPOLATE : SidConfig::INTERPOLATE;
        cfg.sidEmulation = m_sidBuilder;
        cfg.defaultSidModel = ms_isSidModel8580 ? SidConfig::MOS8580 : SidConfig::MOS6581;
        cfg.defaultC64Model = ms_isNtsc ? SidConfig::NTSC : SidConfig::PAL;
        cfg.powerOnDelay = uint16_t(ms_powerOnDelay);

        if (!m_sidplayfp->config(cfg))
        {
            //printf("%s", m_sidplayfp->error());
        }

        if (!m_sidplayfp->load(m_sidTune))
        {
            //printf("%s", m_sidplayfp->error());
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
                m_currentDuration = (uint64_t(m_loops[m_subsongIndex].length) * kSampleRate) / 1000;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        static constexpr uint32_t kCycles = 10000;
        auto numRemainingSamples = numSamples;
        auto numCachedSamples = m_numSamples;

        auto surround = m_surround.Begin();
        while (numRemainingSamples)
        {
            if (numCachedSamples == 0)
            {
                auto numSamplesStereo = m_sidplayfp->play(kCycles);
                if (numSamplesStereo <= 0) // todo error reporting?
                {
                    m_surround.End(surround);
                    m_numSamples = 0;
                    return 0;
                }
                numCachedSamples = uint32_t(numSamplesStereo);
                m_sidplayfp->buffers(&m_samples[0]);
            }
            else
            {
                auto numSamplesToCopy = Min(numRemainingSamples, numCachedSamples);
                if (m_sidplayfp->installedSIDs() == 1)
                {
                    for (uint32_t i = 0; i < numSamplesToCopy; ++i)
                    {
                        auto ss = m_samples[0][i];

                        StereoSample s;
                        s.left = ss.left / 32767.f;
                        s.right = ss.right / 32767.f;
                        output[i] = surround(s);
                    }
                    m_samples[0] += numSamplesToCopy;
                }
                else if (m_sidplayfp->installedSIDs() == 2)
                {
                    for (uint32_t i = 0; i < numSamplesToCopy; ++i)
                    {
                        auto s0 = int(m_samples[0][i].left);
                        auto s1 = int(m_samples[1][i].left);

                        StereoSample s;
                        s.left = (s0 + (s1 >> 1)) / (32767 * 1.41421356237f);
                        s.right = (s1 + (s0 >> 1)) / (32767 * 1.41421356237f);

                        output[i] = surround(s);
                    }
                    m_samples[0] += numSamplesToCopy;
                    m_samples[1] += numSamplesToCopy;
                }
                else
                {
                    for (uint32_t i = 0; i < numSamplesToCopy; ++i)
                    {
                        auto s0 = int(m_samples[0][i].left);
                        auto s1 = int(m_samples[1][i].left);
                        auto s2 = int(m_samples[2][i].left);

                        StereoSample s;
                        s.left = (s0 + s1 + (s2 >> 1)) / (32767 * 1.73205080757f);
                        s.right = (s1 + s2 + (s0 >> 1)) / (32767 * 1.73205080757f);

                        output[i] = surround(s);
                    };
                    m_samples[0] += numSamplesToCopy;
                    m_samples[1] += numSamplesToCopy;
                    m_samples[2] += numSamplesToCopy;
                }
                numRemainingSamples -= numSamplesToCopy;
                numCachedSamples -= numSamplesToCopy;
                output += numSamplesToCopy;
            }
        }
        m_surround.End(surround);

        m_numSamples = numCachedSamples;

        return numSamples;
    }

    void ReplaySidPlay::ResetPlayback()
    {
        m_surround.Reset();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
        m_sidplayfp->reset();

        m_numSamples = 0;
    }

    void ReplaySidPlay::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            for (uint16_t i = 0; i < settings->numSongs; i++)
                m_loops[i] = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
        }

        uint16_t powerOnDelay = uint16_t(settings && settings->overridePowerOnDelay ? settings->powerOnDelay : ms_powerOnDelay);
        bool isSidModelForced = settings && settings->overrideSidModel;
        bool isSidModel8580 = isSidModelForced ? settings->sidModel : ms_isSidModel8580;
        bool isClockForced = settings && settings->overrideClock;
        bool isNtsc = isClockForced ? settings->clock : ms_isNtsc;
        bool isResampling = settings && settings->overrideResampling ? settings->resampling : ms_isResampling;

        for (uint32_t i = 0; i < m_sidplayfp->installedSIDs(); ++i)
            m_sidplayfp->filter(i, (settings && settings->overrideEnableFilter) ? settings->filterEnabled : ms_isFilterEnabled);
        if (!m_isFastSid)
        {
            pCast<ReSIDfpBuilder>(m_sidBuilder)->filter6581Curve(((settings && settings->overrideFilter6581) ? settings->filter6581 : ms_filter6581) / 100.0f);
            pCast<ReSIDfpBuilder>(m_sidBuilder)->filter8580Curve(((settings && settings->overrideFilter8580) ? settings->filter8580 : ms_filter8580) / 100.0f);
            pCast<ReSIDfpBuilder>(m_sidBuilder)->combinedWaveformsStrength(SidConfig::sid_cw_t((settings && settings->overrideCombinedWaveforms) ? settings->combinedWaveforms : ms_combinedWaveforms));
            pCast<ReSIDfpBuilder>(m_sidBuilder)->enableOld6581caps((settings && settings->overrideOld6581caps) ? settings->old6581capsEnabled : ms_isOld6581capsEnable);
            int ofs = 0; // extra params are stacked after the loops if enabled
            pCast<ReSIDfpBuilder>(m_sidBuilder)->dacLeakage(double((settings && settings->overrideDACLeakage) ? pcCast<int32_t>(settings->loops + settings->numSongs)[ofs++] : ms_DACLeakage) / INT_MAX);
            pCast<ReSIDfpBuilder>(m_sidBuilder)->offset6581(double((settings && settings->override6581WaveOffset) ? pcCast<int32_t>(settings->loops + settings->numSongs)[ofs++] : ms_6581WaveOffset) / INT_MAX);
            pCast<ReSIDfpBuilder>(m_sidBuilder)->dcbRes(double((settings && settings->overrideDCBlockerResistance) ? pcCast<int32_t>(settings->loops + settings->numSongs)[ofs++] : ms_DCBlockerResistance) / INT_MAX);
        }

        if (m_currentPosition == 0 && (m_isSidModelForced != isSidModelForced || m_isSidModel8580 != isSidModel8580 || m_isClockForced != isClockForced || m_isNtsc != isNtsc || m_powerOnDelay != powerOnDelay))
        {
            SidConfig cfg = m_sidplayfp->config();
            cfg.samplingMethod = isResampling ? SidConfig::RESAMPLE_INTERPOLATE : SidConfig::INTERPOLATE;
            cfg.defaultSidModel = isSidModel8580 ? SidConfig::MOS8580 : SidConfig::MOS6581;
            cfg.defaultC64Model = isNtsc ? SidConfig::NTSC : SidConfig::PAL;
            cfg.forceSidModel = isSidModelForced;
            cfg.forceC64Model = isClockForced;
            cfg.powerOnDelay = powerOnDelay;
            if (!m_sidplayfp->config(cfg))
            {
                //printf("%s", m_sidplayfp[sidIndex]->error());
            }
            m_sidplayfp->reset();
        }

        m_isSidModelForced = isSidModelForced;
        m_isSidModel8580 = isSidModel8580;
        m_isClockForced = isClockForced;
        m_isNtsc = isNtsc;
        m_isResampling = isResampling;
        m_powerOnDelay = powerOnDelay;

        bool isSurroundEnable = (settings && settings->overrideSurround) ? settings->surround : ms_surround;
        m_surround.Enable(isSurroundEnable);
        if (m_sidplayfp->installedSIDs() == 1)
            m_sidplayfp->surround(isSurroundEnable);
    }

    void ReplaySidPlay::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();

        m_sidTune->selectSong(subsongIndex + 1);
        m_sidplayfp->load(m_sidTune);
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
        return m_loops[m_subsongIndex].GetDuration();
    }

    uint32_t ReplaySidPlay::GetNumSubsongs() const
    {
        return m_sidTune->getInfo()->songs();
    }

    std::string ReplaySidPlay::GetExtraInfo() const
    {
        std::string metadata;
        auto info = m_sidTune->getInfo();
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
        sprintf(txt, "%d channels", 3 * m_sidTune->getInfo()->sidChips());
        info = txt;
        for (int i = 0; i < m_sidTune->getInfo()->sidChips(); i++)
        {
            info += i == 0 ? " " : "/";
            if (m_isSidModelForced)
                info += m_isSidModel8580 ? "8580" : "6581";
            else
                info += m_sidTune->getInfo()->sidModel(i) == SidTuneInfo::SIDMODEL_6581 ? "6581" : "8580";
        }
        info += "\n";
        info += m_sidTune->getInfo()->formatString();
        info += "\n" PACKAGE_STRING;
        if (m_isFastSid)
            info += "|SIDLite";
        else
        {
            info += "|reSIDfp ";
            info += residfp_version_string;
        }
        return info;
    }

    void ReplaySidPlay::SetupMetadata(CommandBuffer metadata)
    {
        auto numSongs = m_sidTune->getInfo()->songs();
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongs == numSongs)
        {
            for (uint32_t i = 0; i < numSongs; i++)
                m_loops[i] = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
        }
        else
        {
            uint32_t value[2] = { settings ? settings->value[0] : 0, settings ? settings->value[1] : 0 };
            int ofs = 0; // extra params are stacked after the loops if enabled
            int32_t extra[3] = {
                settings && settings->overrideDACLeakage ? pcCast<int32_t>(settings->loops + settings->numSongs)[ofs++] : ms_DACLeakage,
                settings && settings->override6581WaveOffset ? pcCast<int32_t>(settings->loops + settings->numSongs)[ofs++] : ms_6581WaveOffset,
                settings && settings->overrideDCBlockerResistance ? pcCast<int32_t>(settings->loops + settings->numSongs)[ofs++] : ms_DCBlockerResistance
            };
            settings = metadata.Create<Settings>(sizeof(Settings) + numSongs * sizeof(LoopInfo) + ofs * sizeof(int32_t));
            settings->value[0] = value[0];
            settings->value[1] = value[1];
            settings->numSongs = numSongs;
            ofs = 0;
            if (settings->overrideDACLeakage)
                pCast<int32_t>(settings->loops + settings->numSongs)[ofs++] = extra[ofs];
            if (settings->override6581WaveOffset)
                pCast<int32_t>(settings->loops + settings->numSongs)[ofs++] = extra[ofs];
            if (settings->overrideDCBlockerResistance)
                pCast<int32_t>(settings->loops + settings->numSongs)[ofs++] = extra[ofs];

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

            char md5[SidTune::MD5_LENGTH + 1];
            m_sidTune->createMD5New(md5);

            for (uint32_t i = 0; i < numSongs; i++)
            {
                auto duration = sidDatabase->lengthMs(md5, i + 1);
                if (duration == -1)
                    duration = kDefaultSongDuration;
                m_loops[i] = settings->loops[i] = { 0, uint32_t(duration) };
            }
        }
    }
}
// namespace rePlayer