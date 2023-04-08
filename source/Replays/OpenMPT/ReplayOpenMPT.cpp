#include "ReplayOpenMPT.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "libopenmpt/libopenmpt.h"
#include "libopenmpt/libopenmpt_version.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::OpenMPT,
        .name = "OpenMPT",
        .about = "OpenMPT " OPENMPT_API_VERSION_STRING "\nCopyright (c) 2004-2023 OpenMPT Project Developers and Contributors\nCopyright (c) 1997-2003 Olivier Lapicque",
        .init = ReplayOpenMPT::Init,
        .release = ReplayOpenMPT::Release,
        .load = ReplayOpenMPT::Load,
        .displaySettings = ReplayOpenMPT::DisplaySettings,
        .editMetadata = ReplayOpenMPT::Settings::Edit
    };

    ReplayOpenMPT::GlobalSettings ReplayOpenMPT::ms_settings[] = {
        GlobalSettings("Default", "Default", true, true),
        GlobalSettings("FastTracker", "FastTracker", false, true),
        GlobalSettings("ImpulseTracker", "Impulse Tracker", false, true),
        GlobalSettings("ScreamTracker3", "Scream Tracker 3", false, true),
        GlobalSettings("ScreamTracker2", "Scream Tracker 2"),
        GlobalSettings("MultiTracker", "MultiTracker"),
        GlobalSettings("DigitalTracker", "Digital Tracker", false, true)
    };

    bool ReplayOpenMPT::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        for (auto& settings : ms_settings)
        {
            settings.labels[0] = std::string("ReplayOpenMPTEnabled_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.isEnabled, settings.labels[0].c_str());
            settings.labels[1] = std::string("ReplayOpenMPTInterpolation_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.interpolation, settings.labels[1].c_str());
            settings.labels[2] = std::string("ReplayOpenMPTRamping_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.ramping, settings.labels[2].c_str());
            settings.labels[3] = std::string("ReplayOpenMPTStereoSeparation_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.stereoSeparation, settings.labels[3].c_str());
            settings.labels[4] = std::string("ReplayOpenMPTSurround_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.surround, settings.labels[4].c_str());
            settings.labels[5] = std::string("ReplayOpenMPTTiming_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.vblank, settings.labels[5].c_str());
        }

        g_replayPlugin.extensions = openmpt_get_supported_extensions();

        return false;
    }

    void ReplayOpenMPT::Release()
    {
        openmpt_free_string(g_replayPlugin.extensions);
    }

    Replay* ReplayOpenMPT::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        openmpt_module_initial_ctl ctrls[] = { { "play.at_end", "continue" }, { nullptr, nullptr } };

        struct CB
        {
            static void log(const char* message, void* user)
            {
                if (strstr(message, "Synthesized MED instruments are not supported."))
                    reinterpret_cast<CB*>(user)->isSynthMed = true;
            }

            bool isSynthMed = false;
        } cb;
        auto module = openmpt_module_create2({ OnRead, OnSeek, OnTell }, stream, cb.log, &cb, nullptr, nullptr, nullptr, nullptr, ctrls);
        if (!module)
            return nullptr;
        if (cb.isSynthMed) // not supported, so let another replay (uade) handle it
        {
            Log::Warning("OpenMPT: Synthesized MED instruments detected, skipping \"%s\"\n", stream->GetName().c_str());
            openmpt_module_destroy(module);
            return nullptr;
        }
        openmpt_module_set_log_func(module, openmpt_log_func_silent, nullptr);

        return new ReplayOpenMPT(module);
    }

    bool ReplayOpenMPT::DisplaySettings()
    {
        bool changed = false;
        if (ImGui::CollapsingHeader("OpenMPT " OPENMPT_API_VERSION_STRING, ImGuiTreeNodeFlags_None))
        {
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PushID("OpenMPT");
            struct
            {
                int32_t index{ 0 };
                static bool getter(void*, int32_t index, const char** outText) { *outText = ms_settings[index].format; return true; }
            } static cb;
            ImGui::Combo("Format", &cb.index, cb.getter, nullptr, _countof(ms_settings));
            if (cb.index != 0)
            {
                const char* const overridden[] = { "Disable", "Enable" };
                if (ImGui::Combo("Override", &ms_settings[cb.index].isEnabled, overridden, _countof(overridden)))
                {
                    ms_settings[cb.index].interpolation = ms_settings[0].interpolation;
                    ms_settings[cb.index].ramping = ms_settings[0].ramping;
                    ms_settings[cb.index].stereoSeparation = ms_settings[0].stereoSeparation;
                    ms_settings[cb.index].surround = ms_settings[0].surround;
                    ms_settings[cb.index].vblank = ms_settings[0].vblank;
                    changed = true;
                }
            }
            auto settingsIndex = ms_settings[cb.index].isEnabled ? cb.index : 0;
            ImGui::BeginDisabled(!ms_settings[cb.index].isEnabled);
            const char* const interpolation[] = { "default", "off", "linear", "cubic", "sinc" };
            changed |= ImGui::Combo("Interpolation", &ms_settings[settingsIndex].interpolation, interpolation, _countof(interpolation));
            changed |= ImGui::SliderInt("Ramping", &ms_settings[settingsIndex].ramping, -1, 10, "%d", ImGuiSliderFlags_NoInput);
            changed |= ImGui::SliderInt("Stereo", &ms_settings[settingsIndex].stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
            const char* const surround[] = { "Stereo", "Surround" };
            changed |= ImGui::Combo("Output", &ms_settings[settingsIndex].surround, surround, _countof(surround));
            const char* const vblank[] = { "Auto", "CIA", "VBlank" };
            int32_t vblankValue = ms_settings[settingsIndex].vblank + 1;
            changed |= ImGui::Combo("Timing", &vblankValue, vblank, _countof(vblank));
            ms_settings[settingsIndex].vblank = vblankValue - 1;
            ImGui::EndDisabled();
            if (!ImGui::GetIO().KeyCtrl)
                ImGui::PopID();
        }
        return changed;
    }

    void ReplayOpenMPT::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_settings[0].stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_settings[0].surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Interpolation", GETSET(entry, overrideInterpolation), GETSET(entry, interpolation),
            ms_settings[0].interpolation, "Interpolation: default", "Interpolation: off", "Interpolation: linear", "Interpolation: cubic", "Interpolation: sinc");
        SliderOverride("Ramping", GETSET(entry, overrideRamping), GETSET(entry, ramping),
            ms_settings[0].ramping, -1, 10, "Ramping %d", -1);
        ComboOverride("VBlank", GETSET(entry, overrideVblank), GETSET(entry, vblank),
            ms_settings[0].vblank == 1, "CIA Timing", "VBlank Timing");

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplayOpenMPT::~ReplayOpenMPT()
    {
        openmpt_module_destroy(m_module);
    }

    ReplayOpenMPT::ReplayOpenMPT(openmpt_module* module)
        : Replay(BuildMediaType(module))
        , m_module(module)
        , m_surround(kSampleRate)
    {}

    size_t ReplayOpenMPT::OnRead(void* stream, void* dst, size_t bytes)
    {
        return reinterpret_cast<io::Stream*>(stream)->Read(dst, bytes);
    }

    int32_t ReplayOpenMPT::OnSeek(void* stream, int64_t offset, int32_t whence)
    {
        if (reinterpret_cast<io::Stream*>(stream)->Seek(offset, whence == OPENMPT_STREAM_SEEK_SET ? io::Stream::kSeekBegin : whence == OPENMPT_STREAM_SEEK_CUR ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd) == Status::kOk)
            return 0;
        return -1;
    }

    int64_t ReplayOpenMPT::OnTell(void* stream)
    {
        return int64_t(reinterpret_cast<io::Stream*>(stream)->GetPosition());
    }

    uint32_t ReplayOpenMPT::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_silenceStart)
        {
            auto currentPosition = m_currentPosition;
            auto currentDuration = m_silenceStart;
            if (currentPosition == currentDuration)
            {
                if (!m_isSilenceTriggered)
                {
                    m_isSilenceTriggered = true;
                    return 0;
                }
            }
            else if (currentPosition < currentDuration && (currentPosition + numSamples) >= currentDuration)
            {
                numSamples = uint32_t(currentDuration - currentPosition);
            }
        }
        numSamples = uint32_t(openmpt_module_read_interleaved_float_stereo(m_module, kSampleRate, numSamples, reinterpret_cast<float*>(output)));
        output->Convert(m_surround, numSamples);
        if (m_silenceStart)
        {
            if (numSamples)
            {
                m_currentPosition += numSamples;
            }
            else
            {
                m_currentPosition = 0;
                m_isSilenceTriggered = false;
            }
        }
        return numSamples;
    }

    uint32_t ReplayOpenMPT::Seek(uint32_t timeInMs)
    {
        auto newTime = openmpt_module_set_position_seconds(m_module, timeInMs / 1000.0);
        m_surround.Reset();
        if (m_silenceStart)
        {
            m_currentPosition = uint64_t(newTime * kSampleRate);
            if (m_currentPosition > m_silenceStart)
                m_currentPosition = m_silenceStart;
            m_isSilenceTriggered = false;
        }
        return uint32_t(newTime * 1000);
    }

    void ReplayOpenMPT::ResetPlayback()
    {
        auto numSubsongs = GetNumSubsongs();
        auto setSubsong = [&]()
        {
            if (numSubsongs > 1)
            {
                auto subsongIndex = m_subsongIndex;
                //libopenmpt bug: if the current subsong index > 0 and we restart it after a loop, then the first end song is not triggered
                //so this hack seems to reset the proper internal states
                openmpt_module_select_subsong(m_module, (subsongIndex + 1) % numSubsongs);
                openmpt_module_select_subsong(m_module, subsongIndex);
            }
            else
                openmpt_module_set_position_seconds(m_module, 0.0);
        };
        setSubsong();
        m_surround.Reset();

        // Silence trimmer
        if (m_previousSubsongIndex != m_subsongIndex)
        {
            int16_t samples[1024];
            uint64_t silenceStart = 0;
            uint64_t totalSamples = 0;
            while (auto numSamples = int32_t(openmpt_module_read_mono(m_module, 8000, 1024, samples)))
            {
                auto currentSample = totalSamples;
                totalSamples += numSamples;
                do
                {
                    if (samples[numSamples - 1] != 0)
                    {
                        silenceStart = currentSample + numSamples;
                        break;
                    }
                } while (--numSamples);
            }
            if (silenceStart < totalSamples)
                m_silenceStart = (silenceStart * kSampleRate) / 8000;
            else
                m_silenceStart = 0;
            m_isSilenceTriggered = false;
            m_currentPosition = 0;
            setSubsong();
            m_previousSubsongIndex = m_subsongIndex;
        }
    }

    void ReplayOpenMPT::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();

        auto type = openmpt_module_get_metadata(m_module, "originaltype_long");
        if (*type == 0)
        {
            openmpt_free_string(type);
            type = openmpt_module_get_metadata(m_module, "type_long");
        }
        auto globalSettings = ms_settings;
        for (uint32_t i = 0; i < _countof(ms_settings); i++)
        {
            if (strstr(type, ms_settings[i].format))
            {
                globalSettings += i;
                break;
            }
        }
        openmpt_free_string(type);
        if (!globalSettings->isEnabled)
            globalSettings = ms_settings;

        if (settings && (settings->overrideStereoSeparation))
            openmpt_module_set_render_param(m_module, OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, 100 - settings->stereoSeparation);
        else
            openmpt_module_set_render_param(m_module, OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, 100);

        openmpt_module_set_render_param(m_module, OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, (1 << ((settings && (settings->overrideInterpolation)) ? settings->interpolation : globalSettings->interpolation)) >> 1);
        openmpt_module_set_render_param(m_module, OPENMPT_MODULE_RENDER_VOLUMERAMPING_STRENGTH, (settings && (settings->overrideRamping)) ? int64_t(settings->ramping) - 1 : int64_t(globalSettings->ramping));

        openmpt_module_ctl_set_integer(m_module, "vblank", (settings && (settings->overrideVblank)) ? int64_t(settings->vblank) : int64_t(globalSettings->vblank));

        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globalSettings->surround);
    }

    void ReplayOpenMPT::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    MediaType ReplayOpenMPT::BuildMediaType(openmpt_module* module)
    {
        auto keyType = openmpt_module_get_metadata(module, "type");
        if (strcmp(keyType, "") == 0)
        {
            openmpt_free_string(keyType);
            keyType = openmpt_module_get_metadata(module, "originaltype");
        }
        MediaType mediaType(keyType, eReplay::OpenMPT);
        openmpt_free_string(keyType);
        return mediaType;
    }

    uint32_t ReplayOpenMPT::GetDurationMs() const
    {
        if (m_silenceStart)
            return uint32_t((m_silenceStart * 1000ull) / kSampleRate);
        return uint32_t(openmpt_module_get_duration_seconds(m_module) * 1000ull);
    }

    uint32_t ReplayOpenMPT::GetNumSubsongs() const
    {
        uint32_t numSubsongs = openmpt_module_get_num_subsongs(m_module);
        assert(numSubsongs <= 65536);
        return numSubsongs;
    }

    std::string ReplayOpenMPT::GetExtraInfo() const
    {
        std::string metadata;
        if (auto str = openmpt_module_get_metadata(m_module, "title"))
        {
            if (str[0])
                metadata = str;
            openmpt_free_string(str);
        }
        if (auto str = openmpt_module_get_subsong_name(m_module, m_subsongIndex))
        {
            if (str[0])
            {
                if (metadata.size())
                    metadata += "\n";
                metadata += str;
            }
            openmpt_free_string(str);
        }
        if (auto str = openmpt_module_get_metadata(m_module, "message"))
        {
            if (str[0])
            {
                if (metadata.size())
                    metadata += "\n";
                metadata += str;
            }
            openmpt_free_string(str);
        }
        return metadata;
    }

    std::string ReplayOpenMPT::GetInfo() const
    {
        std::string info;
        char buf[128];
        sprintf(buf, "%d", openmpt_module_get_num_channels(m_module));
        info += buf;
        info += " channels";
        auto type = openmpt_module_get_metadata(m_module, "type_long");
        if (type)
        {
            if (type[0] != 0)
            {
                sprintf(buf, "\n%s", type);
                info += buf;
            }
            openmpt_free_string(type);
        }
        info += "\nOpenMPT " OPENMPT_API_VERSION_STRING;
        return info;
    }
}
// namespace rePlayer