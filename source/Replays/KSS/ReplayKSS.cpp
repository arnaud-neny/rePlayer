#include "ReplayKSS.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <filesystem>

namespace rePlayer
{
    #define LIBKSS_VERSION "LIBKSS 1.2.1"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::KSS,
        .name = "LIBKSS",
        .extensions = "kss;mgs;bgm;opx;mpk;mbm",
        .about = "LIBKSS\nCopyright (c) 2015 Mitsutaka Okazaki",
        .settings = LIBKSS_VERSION,
        .init = ReplayKSS::Init,
        .load = ReplayKSS::Load,
        .displaySettings = ReplayKSS::DisplaySettings,
        .editMetadata = ReplayKSS::Settings::Edit
    };

    bool ReplayKSS::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_masterVolume, "ReplayKSSMasterVolume");
        window.RegisterSerializedData(ms_devicePan, "ReplayKSSDevicePan");
        window.RegisterSerializedData(ms_silence, "ReplayKSSSilence");

        return false;
    }

    Replay* ReplayKSS::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() < 8 || stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;

        Array<uint32_t> subsongs;
        uint32_t fileIndex = 0;
        KSS* kss0 = nullptr;
        std::string title;
        for (SmartPtr<io::Stream> kssStream = stream; kssStream; kssStream = kssStream->Next())
        {
            auto data = kssStream->Read();
            struct Loader
            {
                static void load(void* userData, const char* name, const uint8_t** buffer, size_t* size)
                {
                    auto* This = reinterpret_cast<Loader*>(userData);
                    This->newStream = This->stream->Open(name);
                    if (This->newStream)
                    {
                        auto data = This->newStream->Read();
                        *buffer = data.Items();
                        *size = data.NumItems();
                    }
                }
                io::Stream* stream;
                SmartPtr<io::Stream> newStream;
            } cb{ kssStream };
            if (auto kss = KSS_bin2kss((uint8_t*)data.Items(), uint32_t(data.Size()), kssStream->GetName().c_str(), cb.load, &cb))
            {
                subsongs.Add(fileIndex);
                if (kss0 == nullptr)
                {
                    title = std::filesystem::path(kssStream->GetName()).stem().string();
                    kss0 = kss;
                }
                else
                    KSS_delete(kss);
            }
            fileIndex++;
        }
        if (subsongs.IsNotEmpty())
            return new ReplayKSS(stream, std::move(subsongs), kss0, std::move(title), metadata);
        return nullptr;
    }

    bool ReplayKSS::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Master Volume", &ms_masterVolume, -127, 127, "%d", ImGuiSliderFlags_NoInput);
        changed |= ImGui::SliderInt("Device Pan", &ms_devicePan, -127, 127, "%d", ImGuiSliderFlags_NoInput);
        changed |= ImGui::SliderInt("Silence TimeOut", &ms_silence, 0, 30, "%d sec", ImGuiSliderFlags_NoInput);
        return changed;
    }

    void ReplayKSS::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(LoopInfo));
            entry->loops[0] = {};
        }

        SliderOverride("Master Volume", GETSET(entry, overrideMasterVolume), GETSET(entry, masterVolume),
            ms_masterVolume, -127, 127, "Master Volume %d", -127);
        SliderOverride("Device Pan", GETSET(entry, overrideDevicePan), GETSET(entry, devicePan),
            ms_devicePan, -127, 127, "Device Pan %d", -127);
        Loops(context, entry->loops, entry->numSongsMinusOne + 1, kDefaultSongDuration);
    }

    int32_t ReplayKSS::ms_masterVolume = 32;
    int32_t ReplayKSS::ms_devicePan = 32;
    int32_t ReplayKSS::ms_silence = 5;

    ReplayKSS::~ReplayKSS()
    {
        KSSPLAY_delete(m_kssplay);
        KSS_delete(m_kss);
    }

    static eExtension GetExt(KSS* kss)
    {
        switch (kss->type)
        {
        case KSSDATA:
            return eExtension::_kss;
        case MGSDATA:
            return eExtension::_mgs;
        case MBMDATA:
            return eExtension::_mbm;
        case MPK106DATA:
            return eExtension::_mpk;
        case MPK103DATA:
            return eExtension::_mpk;
        case BGMDATA:
            return eExtension::_bgm;
        case OPXDATA:
            return eExtension::_opx;
        default:
            break;
        }

        return eExtension::_kss;
    }

    ReplayKSS::ReplayKSS(io::Stream* stream, Array<uint32_t>&& subsongs, KSS* kss, std::string&& title, CommandBuffer metadata)
        : Replay(GetExt(kss), eReplay::KSS)
        , m_kss(kss)
        , m_kssplay(KSSPLAY_new(kSampleRate, 2, 16))
        , m_stream(stream)
        , m_subsongs(std::move(subsongs))
        , m_title(std::move(title))
    {
        KSSPLAY_set_data(m_kssplay, kss);
        m_kssplay->opll_stereo = 1;
        SetupMetadata(metadata);
    }

    uint32_t ReplayKSS::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentDuration != 0)
        {
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
        }
        else
        {
            auto loopCount = KSSPLAY_get_loop_count(m_kssplay);
            if (loopCount != m_loopCount || KSSPLAY_get_stop_flag(m_kssplay))
            {
                m_loopCount = loopCount;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        KSSPLAY_calc(m_kssplay, buf, numSamples);
        output->Convert(buf, numSamples);

        return numSamples;
    }

    void ReplayKSS::ResetPlayback()
    {
        if (m_subsongs.NumItems() == 1)
            KSSPLAY_reset(m_kssplay, m_subsongIndex, 0);
        else
        {
            if (m_subsongIndex != m_currentSubsongIndex)
            {
                auto stream = m_stream;
                for (uint32_t fileIndex = 0; stream; fileIndex++)
                {
                    if (fileIndex == m_subsongs[m_subsongIndex])
                    {
                        auto data = stream->Read();
                        KSSPLAY_delete(m_kssplay);
                        KSS_delete(m_kss);

                        struct Loader
                        {
                            static void load(void* userData, const char* name, const uint8_t** buffer, size_t* size)
                            {
                                auto* This = reinterpret_cast<Loader*>(userData);
                                This->newStream = This->stream->Open(name);
                                if (This->newStream)
                                {
                                    auto data = This->newStream->Read();
                                    *buffer = data.Items();
                                    *size = data.NumItems();
                                }
                            }
                            io::Stream* stream;
                            SmartPtr<io::Stream> newStream;
                        } cb{ stream };

                        m_kss = KSS_bin2kss((uint8_t*)data.Items(), uint32_t(data.Size()), stream->GetName().c_str(), cb.load, &cb);
                        m_kssplay = KSSPLAY_new(kSampleRate, 2, 16);

                        KSSPLAY_set_data(m_kssplay, m_kss);
                        m_kssplay->opll_stereo = 1;

                        m_title = std::filesystem::path(stream->GetName()).stem().string();
                        break;
                    }
                    stream = stream->Next();
                }

                m_currentSubsongIndex = m_subsongIndex;
            }
            KSSPLAY_reset(m_kssplay, 0, 0);
        }

        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_PSG, 1);
        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_SCC, 1);
        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_OPL, 1);
        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_OPLL, 1);

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
    }

    void ReplayKSS::ApplySettings(const CommandBuffer metadata)
    {
        auto* settings = metadata.Find<Settings>();
        if (settings)
        {
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_loops[i] = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
        }

        m_kssplay->master_volume = settings && settings->overrideMasterVolume ? settings->masterVolume - 127 : ms_masterVolume;
        int32_t devicePan = settings && settings->overrideDevicePan ? settings->devicePan - 127: ms_devicePan;
        KSSPLAY_set_device_pan(m_kssplay, KSS_DEVICE_PSG, -devicePan);
        KSSPLAY_set_device_pan(m_kssplay, KSS_DEVICE_SCC, devicePan);
        KSSPLAY_set_channel_pan(m_kssplay, KSS_DEVICE_OPLL, 0, 1);
        KSSPLAY_set_channel_pan(m_kssplay, KSS_DEVICE_OPLL, 1, 2);
        KSSPLAY_set_channel_pan(m_kssplay, KSS_DEVICE_OPLL, 2, 1);
        KSSPLAY_set_channel_pan(m_kssplay, KSS_DEVICE_OPLL, 3, 2);
        KSSPLAY_set_channel_pan(m_kssplay, KSS_DEVICE_OPLL, 4, 1);
        KSSPLAY_set_channel_pan(m_kssplay, KSS_DEVICE_OPLL, 5, 2);

        m_kssplay->silent_limit = ms_silence * 1000;
    }

    void ReplayKSS::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayKSS::GetDurationMs() const
    {
        return m_loops[m_subsongIndex].GetDuration();
    }

    uint32_t ReplayKSS::GetNumSubsongs() const
    {
        if (m_subsongs.NumItems() > 1)
            return m_subsongs.NumItems();
        return m_kss->trk_max - m_kss->trk_min + 1;
    }

    std::string ReplayKSS::GetSubsongTitle() const
    {
        if (m_mediaType.ext == eExtension::_mbm)
            return m_title;
        return Replay::GetSubsongTitle();
    }

    std::string ReplayKSS::GetExtraInfo() const
    {
        std::string metadata;
        metadata += KSS_get_title(m_kss);
        return metadata;
    }

    std::string ReplayKSS::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        info += reinterpret_cast<char*>(m_kss->idstr);
        info += m_kss->mode ? " / SEGA\n" LIBKSS_VERSION : " / MSX\n" LIBKSS_VERSION;
        return info;
    }

    void ReplayKSS::SetupMetadata(CommandBuffer metadata)
    {
        uint32_t numSongsMinusOne = GetNumSubsongs() - 1;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongsMinusOne == numSongsMinusOne)
        {
            for (uint32_t i = 0; i <= numSongsMinusOne; i++)
                m_loops[i] = settings->loops[i].GetFixed();
            m_currentDuration = (uint64_t(m_loops[m_subsongIndex].GetDuration()) * kSampleRate) / 1000;
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + (numSongsMinusOne + 1) * sizeof(LoopInfo));
            settings->value = value;
            settings->numSongsMinusOne = numSongsMinusOne;
            for (uint16_t i = 0; i <= numSongsMinusOne; i++)
            {
                settings->loops[i] = {};
                m_loops[i] = {};
            }
            m_currentDuration = 0;
        }
    }
}
// namespace rePlayer