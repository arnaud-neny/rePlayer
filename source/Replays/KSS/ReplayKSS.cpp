#include "ReplayKSS.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    #define LIBKSS_VERSION "LIBKSS 1.2.0"

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
        auto data = stream->Read();
        if (auto kss = KSS_bin2kss((uint8_t*)data.Items(), uint32_t(data.Size()), stream->GetName().c_str()))
            return new ReplayKSS(kss, metadata);

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
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(uint32_t));
            entry->GetDurations()[0] = 0;
        }

        SliderOverride("Master Volume", GETSET(entry, overrideMasterVolume), GETSET(entry, masterVolume),
            ms_masterVolume, -127, 127, "Master Volume %d", -127);
        SliderOverride("Device Pan", GETSET(entry, overrideDevicePan), GETSET(entry, devicePan),
            ms_devicePan, -127, 127, "Device Pan %d", -127);

        const float buttonSize = ImGui::GetFrameHeight();
        auto durations = entry->GetDurations();
        for (uint16_t i = 0; i <= entry->numSongsMinusOne; i++)
        {
            ImGui::PushID(i);
            bool isEnabled = durations[i] != 0;
            uint32_t duration = isEnabled ? durations[i] : kDefaultSongDuration;
            auto pos = ImGui::GetCursorPosX();
            if (ImGui::Checkbox("##Checkbox", &isEnabled))
                duration = kDefaultSongDuration;
            ImGui::SameLine();
            ImGui::BeginDisabled(!isEnabled);
            char txt[64];
            sprintf(txt, "Subsong #%d Duration", i + 1);
            auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4 - buttonSize;
            ImGui::SetNextItemWidth(2.0f * width / 3.0f - ImGui::GetCursorPosX() + pos);
            ImGui::DragUint("##Duration", &duration, 1000.0f, 1, 0xffFFffFF, txt, ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            int32_t milliseconds = duration % 1000;
            int32_t seconds = (duration / 1000) % 60;
            int32_t minutes = duration / 60000;
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3 - buttonSize;
            ImGui::SetNextItemWidth(width / 3.0f);
            ImGui::DragInt("##Minutes", &minutes, 0.1f, 0, 65535, "%d m", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2 - buttonSize;
            ImGui::SetNextItemWidth(width / 2.0f);
            ImGui::DragInt("##Seconds", &seconds, 0.1f, 0, 59, "%d s", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x - buttonSize;
            ImGui::SetNextItemWidth(width);
            ImGui::DragInt("##Milliseconds", &milliseconds, 1.0f, 0, 999, "%d ms", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::Button("E", ImVec2(buttonSize, 0.0f)))
            {
                context.duration = duration;
                context.songIndex = i;
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && context.songIndex == i)
            {
                milliseconds = context.duration % 1000;
                seconds = (context.duration / 1000) % 60;
                minutes = context.duration / 60000;
                context.duration = 0;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Open Waveform Viewer");
            ImGui::EndDisabled();
            durations[i] = isEnabled ? uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds) : 0;
            ImGui::PopID();
        }

        context.metadata.Update(entry, false);
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

    ReplayKSS::ReplayKSS(KSS* kss, CommandBuffer metadata)
        : Replay(GetExt(kss), eReplay::KSS)
        , m_kss(kss)
        , m_kssplay(KSSPLAY_new(kSampleRate, 2, 16))
    {
        KSSPLAY_set_data(m_kssplay, kss);
        m_kssplay->opll_stereo = 1;
        SetupMetadata(metadata);
    }

    uint32_t ReplayKSS::Render(StereoSample* output, uint32_t numSamples)
    {
        auto loopCount = KSSPLAY_get_loop_count(m_kssplay);
        if (loopCount != m_loopCount || KSSPLAY_get_stop_flag(m_kssplay))
        {
            m_loopCount = loopCount;
            return 0;
        }

        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
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
        KSSPLAY_reset(m_kssplay, m_subsongIndex, 0);

        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_PSG, 1);
        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_SCC, 1);
        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_OPL, 1);
        KSSPLAY_set_device_quality(m_kssplay, KSS_DEVICE_OPLL, 1);

        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
    }

    void ReplayKSS::ApplySettings(const CommandBuffer metadata)
    {
        auto* settings = metadata.Find<Settings>();
        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_durations[i] = durations[i];
            m_currentDuration = (uint64_t(m_durations[m_subsongIndex]) * kSampleRate) / 1000;
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

    void ReplayKSS::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayKSS::GetDurationMs() const
    {
        return m_durations[m_subsongIndex];
    }

    uint32_t ReplayKSS::GetNumSubsongs() const
    {
        return m_kss->trk_max - m_kss->trk_min + 1;
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
        uint32_t numSongsMinusOne = m_kss->trk_max - m_kss->trk_min;
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
                durations[i] = 0;
                m_durations[i] = 0;
            }
            m_currentDuration = 0;
        }
    }
}
// namespace rePlayer