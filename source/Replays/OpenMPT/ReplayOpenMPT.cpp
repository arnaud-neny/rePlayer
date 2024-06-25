// Some minor changes are done in OpenMPT:
// - libopenmpt/libopenmpt.h
// - libopenmpt/libopenmpt_c.cpp
// - libopenmpt/libopenmpt_impl.cpp
// - libopenmpt/libopenmpt_impl.hpp
#include "ReplayOpenMPT.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <Thread/Thread.h>

#include <ReplayDll.h>

#include "libopenmpt/libopenmpt_version.h"

#include <atomic>

#define OPENMPT_REVISION "r21056"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::OpenMPT,
        .name = "OpenMPT",
        .about = "OpenMPT " OPENMPT_API_VERSION_STRING OPENMPT_REVISION "\nCopyright (c) 2004-2024 OpenMPT Project Developers and Contributors\nCopyright (c) 1997-2003 Olivier Lapicque",
        .settings = "OpenMPT " OPENMPT_API_VERSION_STRING OPENMPT_REVISION,
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
            settings.labels[6] = std::string("ReplayOpenMPTPatterns_") + std::string(settings.serializedName);
            window.RegisterSerializedData(settings.patterns, settings.labels[6].c_str());
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

        auto* module = openmpt_module_create2({ OnRead, OnSeek, OnTell }, stream, openmpt_log_func_silent, nullptr, nullptr, nullptr, nullptr, nullptr, ctrls);
        if (!module)
            return nullptr;

        openmpt_module_initial_ctl emptyCtrls[] = { { "play.at_end", "continue" }, { "load.skip_samples", "1" }, { nullptr, nullptr } };
        auto data = stream->Read();
        return new ReplayOpenMPT(stream, module, openmpt_module_ext_create_from_memory(data.Items(), data.Size(), openmpt_log_func_silent, nullptr, nullptr, nullptr, nullptr, nullptr, emptyCtrls));
    }

    bool ReplayOpenMPT::DisplaySettings()
    {
        bool changed = false;
        struct
        {
            int32_t index = 0;
            static const char* getter(void*, int32_t index) { return ms_settings[index].format; }
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
                ms_settings[cb.index].patterns = ms_settings[0].patterns;
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
        const char* const patterns[] = { "Disable", "Enable" };
        changed |= ImGui::Combo("Show Patterns", &ms_settings[settingsIndex].patterns, patterns, _countof(patterns));
        ImGui::EndDisabled();
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
        openmpt_module_ext_destroy(m_moduleExtVisuals);
        openmpt_module_destroy(m_modulePlayback);

        std::atomic_ref(m_isSilenceDetectionCancelled).store(true);
        while (std::atomic_ref(m_isSilenceDetectionRunning).load())
            thread::Sleep(0);
    }

    ReplayOpenMPT::ReplayOpenMPT(io::Stream* stream, openmpt_module* modulePlayback, openmpt_module_ext* moduleVisuals)
        : Replay(BuildMediaType(modulePlayback))
        , m_stream(stream->Clone())
        , m_modulePlayback(modulePlayback)
        , m_moduleVisuals(openmpt_module_ext_get_module(moduleVisuals))
        , m_moduleExtVisuals(moduleVisuals)
        , m_surround(kSampleRate)
    {
        openmpt_module_ext_get_interface(moduleVisuals, LIBOPENMPT_EXT_C_INTERFACE_PATTERN_VIS, &m_modulePatternVis, sizeof(m_modulePatternVis));
    }

    size_t ReplayOpenMPT::OnRead(void* stream, void* dst, size_t bytes)
    {
        return size_t(reinterpret_cast<io::Stream*>(stream)->Read(dst, bytes));
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
        auto currentPosition = m_currentPosition;
        uint64_t currentDuration = 0;
        auto hasDuration = !std::atomic_ref(m_silenceStart).compare_exchange_strong(currentDuration, currentDuration);
        if (hasDuration)
        {
            if (currentPosition >= currentDuration)
            {
                if (!m_isSilenceTriggered)
                {
                    m_isSilenceTriggered = true;
                    return 0;
                }
                currentPosition = currentPosition % currentDuration;
            }
            if ((currentPosition + numSamples) >= currentDuration)
            {
                numSamples = uint32_t(currentDuration - currentPosition);
            }
        }
        numSamples = uint32_t(openmpt_module_read_interleaved_float_stereo(m_modulePlayback, kSampleRate, numSamples, reinterpret_cast<float*>(output)));
        output->Convert(m_surround, numSamples);
        if (numSamples)
        {
            m_currentPosition += numSamples;
        }
        else if (hasDuration)
        {
            m_currentPosition = 0;
            m_isSilenceTriggered = false;
        }
        return numSamples;
    }

    uint32_t ReplayOpenMPT::Seek(uint32_t timeInMs)
    {
        auto newTime = openmpt_module_set_position_seconds(m_modulePlayback, timeInMs / 1000.0);
        m_surround.Reset();
        m_currentPosition = uint64_t(newTime * kSampleRate);
        uint64_t currentDuration = 0;
        if (!std::atomic_ref(m_silenceStart).compare_exchange_strong(currentDuration, currentDuration) && m_currentPosition > currentDuration)
            m_currentPosition = currentDuration;
        m_isSilenceTriggered = false;

        auto posInSec = openmpt_module_get_position_seconds(m_moduleVisuals) * 1000.0;
        if (posInSec > timeInMs)
        {
            posInSec = 0.0;
            m_visualsSamples = 0;
            openmpt_module_set_position_seconds(m_moduleVisuals, 0.0);
        }
        if (auto numSamples = uint32_t((timeInMs - posInSec) * kSampleRate / 1000.0))
        {
            auto availableSamples = Min(numSamples, m_visualsSamples);
            m_visualsSamples -= availableSamples;
            numSamples -= availableSamples;
            while (numSamples)
            {
                m_visualsSamples = uint32_t(openmpt_module_read_one_tick(m_moduleVisuals, kSampleRate));
                availableSamples = Min(numSamples, m_visualsSamples);
                m_visualsSamples -= availableSamples;
                numSamples -= availableSamples;
            }
        }

        return uint32_t(newTime * 1000);
    }

    void ReplayOpenMPT::ResetPlayback()
    {
        auto numSubsongs = GetNumSubsongs();
        auto setSubsong = [&](auto* module)
        {
            if (numSubsongs > 1)
            {
                auto subsongIndex = m_subsongIndex;
                //libopenmpt bug: if the current subsong index > 0 and we restart it after a loop, then the first end song is not triggered
                //so this hack seems to reset the proper internal states
                openmpt_module_select_subsong(module, (subsongIndex + 1) % numSubsongs);
                openmpt_module_select_subsong(module, subsongIndex);
            }
            else
                openmpt_module_set_position_seconds(module, 0.0);
        };
        setSubsong(m_modulePlayback);
        setSubsong(m_moduleVisuals);
        m_surround.Reset();

        // Silence trimmer
        if (m_previousSubsongIndex != m_subsongIndex)
        {
            std::atomic_ref(m_isSilenceDetectionCancelled).store(true);
            while (std::atomic_ref(m_isSilenceDetectionRunning).load())
                thread::Sleep(0);
            m_isSilenceDetectionCancelled = false;
            m_isSilenceDetectionRunning = true;
            m_silenceStart = 0;
            g_replayPlugin.addJob(this, ReplayPlugin::JobCallback(SilenceDetection));
            m_previousSubsongIndex = m_subsongIndex;
        }

        m_isSilenceTriggered = false;
        m_currentPosition = 0;
        m_visualsSamples = 0;
    }

    void ReplayOpenMPT::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();

        auto type = openmpt_module_get_metadata(m_modulePlayback, "originaltype_long");
        if (*type == 0)
        {
            openmpt_free_string(type);
            type = openmpt_module_get_metadata(m_modulePlayback, "type_long");
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

        openmpt_module_set_render_param(m_modulePlayback, OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globalSettings->stereoSeparation);
        openmpt_module_set_render_param(m_modulePlayback, OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, (1 << ((settings && settings->overrideInterpolation) ? settings->interpolation : globalSettings->interpolation)) >> 1);
        openmpt_module_set_render_param(m_modulePlayback, OPENMPT_MODULE_RENDER_VOLUMERAMPING_STRENGTH, (settings && settings->overrideRamping) ? int64_t(settings->ramping) - 1 : int64_t(globalSettings->ramping));

        openmpt_module_ctl_set_integer(m_modulePlayback, "vblank", (settings && (settings->overrideVblank)) ? int64_t(settings->vblank) : int64_t(globalSettings->vblank));
        openmpt_module_ctl_set_integer(m_moduleVisuals, "vblank", (settings && (settings->overrideVblank)) ? int64_t(settings->vblank) : int64_t(globalSettings->vblank));

        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globalSettings->surround);

        m_arePatternsDisplayed = globalSettings->patterns;
    }

    void ReplayOpenMPT::SetSubsong(uint32_t subsongIndex)
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
        return uint32_t(openmpt_module_get_duration_seconds(m_modulePlayback) * 1000ull);
    }

    uint32_t ReplayOpenMPT::GetNumSubsongs() const
    {
        uint32_t numSubsongs = openmpt_module_get_num_subsongs(m_modulePlayback);
        assert(numSubsongs <= 65536);
        return numSubsongs;
    }

    std::string ReplayOpenMPT::GetExtraInfo() const
    {
        std::string metadata;
        if (auto str = openmpt_module_get_metadata(m_modulePlayback, "title"))
        {
            if (str[0])
                metadata = str;
            openmpt_free_string(str);
        }
        if (auto str = openmpt_module_get_subsong_name(m_modulePlayback, m_subsongIndex))
        {
            if (str[0])
            {
                if (metadata.size())
                    metadata += "\n";
                metadata += str;
            }
            openmpt_free_string(str);
        }
        if (auto str = openmpt_module_get_metadata(m_modulePlayback, "message"))
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
        sprintf(buf, "%d", openmpt_module_get_num_channels(m_modulePlayback));
        info = buf;
        info += " channels\n";
        auto type = openmpt_module_get_metadata(m_modulePlayback, "type_long");
        if (type)
        {
            info += type;
            openmpt_free_string(type);
        }
        info += "\nOpenMPT " OPENMPT_API_VERSION_STRING OPENMPT_REVISION;
        return info;
    }

    Replay::Patterns ReplayOpenMPT::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        if (numSamples > 0)
        {
            auto availableSamples = Min(numSamples, m_visualsSamples);
            m_visualsSamples -= availableSamples;
            numSamples -= availableSamples;
            while (numSamples)
            {
                m_visualsSamples = uint32_t(openmpt_module_read_one_tick(m_moduleVisuals, kSampleRate));
                availableSamples = Min(numSamples, m_visualsSamples);
                m_visualsSamples -= availableSamples;
                numSamples -= availableSamples;
            }
        }

        if (!m_arePatternsDisplayed && !(flags & Patterns::kEnablePatterns)) // kEnablePatterns has priority over internal flag
            return {};

        auto numChannels = openmpt_module_get_num_channels(m_moduleVisuals);

        Patterns patterns;
        patterns.lines.Reserve(65536);
        patterns.sizes.Resize(numLines);
        patterns.currentLine = openmpt_module_get_current_row(m_moduleVisuals);
        if (flags & (Patterns::kEnableInstruments | Patterns::kEnableVolume | Patterns::kEnableEffects))
            patterns.width = (numChannels * 4 - 1) * charWidth;
        else
            patterns.width = (numChannels * 3) * charWidth + (numChannels - 1) * spaceWidth;
        uint16_t size = uint16_t(numChannels * 4 - 1);
        if (flags & Patterns::kEnableRowNumbers)
        {
            patterns.width += 4 * charWidth;
            size += uint16_t(4);
        }
        if (flags & Patterns::kEnableInstruments)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }
        if (flags & Patterns::kEnableVolume)
        {
            patterns.width += numChannels * 3 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 4);
        }
        if (flags & Patterns::kEnableEffects)
        {
            patterns.width += numChannels * 3 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 4);
        }

        auto currentPattern = openmpt_module_get_current_pattern(m_moduleVisuals);
        auto numRows = openmpt_module_get_pattern_num_rows(m_moduleVisuals, currentPattern);
        auto currentRow = patterns.currentLine - int32_t(numLines) / 2;

        for (uint32_t line = 0; line < numLines; currentRow++, line++)
        {
            if (currentRow >= 0 && currentRow < numRows)
            {
                char color = Patterns::kColorDefault;
                if (flags & Patterns::kEnableRowNumbers)
                {
                    if (flags & Patterns::kEnableColors)
                        patterns.lines.Add(0x80 | color);
                    patterns.lines.Add('0' + ((currentRow / 100) % 10));
                    patterns.lines.Add('0' + ((currentRow / 10) % 10));
                    patterns.lines.Add('0' + (currentRow % 10));
                    patterns.lines.Add('\x7f');
                }

                for (int32_t channel = 0;;)
                {
                    auto read = [&](int command, int numChars, bool doColor = false, Patterns::ColorIndex otherColor = Patterns::kColorDefault)
                    {
                        auto* in = openmpt_module_format_pattern_row_channel_command(m_moduleVisuals, currentPattern, currentRow, channel, command);
                        if (doColor) switch (*in)
                        {
                        case ' ':
                        case '.':
                        case '^':
                        case '=':
                        case '~':
                            otherColor = Patterns::kColorEmpty;
                        default:
                            if (color != otherColor)
                            {
                                color = otherColor;
                                patterns.lines.Add(0x80 | color);
                            }
                        }

                        for (auto* c = in; *c && numChars--; c++)
                        {
                            if (*c <= ' ')
                                patterns.lines.Add('\x7f'); // invisible character
                            else
                                patterns.lines.Add(*c);
                        }
                        openmpt_free_string(in);
                        while (numChars-- > 0)
                            patterns.lines.Add('\x7f'); // invisible character
                    };
                    auto getColor = [](int effectType) {
                        switch (effectType)
                        {
                        case OPENMPT_MODULE_EXT_INTERFACE_PATTERN_VIS_EFFECT_TYPE_UNKNOWN:
                            return Patterns::kColorDefault;
                        case OPENMPT_MODULE_EXT_INTERFACE_PATTERN_VIS_EFFECT_TYPE_GENERAL:
                            return Patterns::kColorText;
                        case OPENMPT_MODULE_EXT_INTERFACE_PATTERN_VIS_EFFECT_TYPE_GLOBAL:
                            return Patterns::kColorGlobal;
                        case OPENMPT_MODULE_EXT_INTERFACE_PATTERN_VIS_EFFECT_TYPE_VOLUME:
                            return Patterns::kColorVolume;
                        case OPENMPT_MODULE_EXT_INTERFACE_PATTERN_VIS_EFFECT_TYPE_PANNING:
                            return Patterns::kColorInstrument;
                        case OPENMPT_MODULE_EXT_INTERFACE_PATTERN_VIS_EFFECT_TYPE_PITCH:
                            return Patterns::kColorPitch;
                        }
                        return Patterns::kColorDefault;
                    };

                    read(OPENMPT_MODULE_COMMAND_NOTE, 3, flags & Patterns::kEnableColors, Patterns::kColorText);
                    if (flags & Patterns::kEnableInstruments)
                    {
                        patterns.lines.Add(' ');
                        read(OPENMPT_MODULE_COMMAND_INSTRUMENT, 2, flags & Patterns::kEnableColors, Patterns::kColorInstrument);
                    }
                    if (flags & Patterns::kEnableVolume)
                    {
                        patterns.lines.Add(' ');
                        read(OPENMPT_MODULE_COMMAND_VOLUMEEFFECT, 1, flags & Patterns::kEnableColors, getColor(m_modulePatternVis.get_pattern_row_channel_volume_effect_type(m_moduleExtVisuals, currentPattern, currentRow, channel)));
                        read(OPENMPT_MODULE_COMMAND_VOLUME, 2);
                    }
                    if (flags & Patterns::kEnableEffects)
                    {
                        patterns.lines.Add(' ');
                        read(OPENMPT_MODULE_COMMAND_EFFECT, 1, flags & Patterns::kEnableColors, getColor(m_modulePatternVis.get_pattern_row_channel_effect_type(m_moduleExtVisuals, currentPattern, currentRow, channel)));
                        read(OPENMPT_MODULE_COMMAND_PARAMETER, 2);
                    }
                    if (++channel == numChannels)
                        break;
                    if (flags & (Patterns::kEnableInstruments | Patterns::kEnableVolume | Patterns::kEnableEffects))
                        patterns.lines.Add('\x7f');
                    else
                        patterns.lines.Add(' ');
                }

                patterns.sizes[line] = size;
            }
            else
                patterns.sizes[line] = 0;
            patterns.lines.Add('\0');
        }
        return patterns;
    }

    void ReplayOpenMPT::SilenceDetection(ReplayOpenMPT* replay)
    {
        replay->m_stream->Rewind();
        openmpt_module_initial_ctl ctrls[] = { { "play.at_end", "continue" }, { nullptr, nullptr } };
        auto* module = openmpt_module_create2({ OnRead, OnSeek, OnTell }, replay->m_stream, openmpt_log_func_silent, nullptr, nullptr, nullptr, nullptr, nullptr, ctrls);
        openmpt_module_select_subsong(module, replay->m_subsongIndex);

        static constexpr uint32_t kSilenceSampleRate = 8000;
        Array<float> samples(kSilenceSampleRate, uint32_t((openmpt_module_get_duration_seconds(module) + 7) * kSilenceSampleRate));
        uint32_t totalSamples = 0;
        while (auto numSamples = int32_t(openmpt_module_read_float_mono(module, kSilenceSampleRate, kSilenceSampleRate, samples.Items(totalSamples))))
        {
            totalSamples += numSamples;

            if (std::atomic_ref(replay->m_isSilenceDetectionCancelled).load())
                break;

            samples.Push(kSilenceSampleRate);
        }
        openmpt_module_destroy(module);

        auto min = samples[totalSamples - 1];
        auto max = min;
        auto silenceStart = totalSamples - 2;
        while (silenceStart > 0)
        {
            static constexpr float kEpsilon = 1.5f / 32767.0f;

            auto sample = samples[silenceStart--];
            if (sample < min)
            {
                if (max - sample > kEpsilon)
                    break;
                min = sample;
            }
            else if (sample > max)
            {
                if (sample - min > kEpsilon)
                    break;
                max = sample;
            }
        }
        silenceStart += kSilenceSampleRate / 4; // keep 0.25 second of silence
        std::atomic_ref(replay->m_silenceStart).exchange(silenceStart < totalSamples ? MulDiv(silenceStart, kSampleRate, kSilenceSampleRate) : 0);
        std::atomic_ref(replay->m_isSilenceDetectionRunning).store(false);
    }
}
// namespace rePlayer