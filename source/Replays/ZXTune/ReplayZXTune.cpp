#include "ReplayZXTune.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <core/data_location.h>
#include "core/module_detect.h"
#include <core/plugins/player_plugin.h>
#include <core/plugin.h>
#include <core/service.h>
#include <module/attributes.h>
#include <module/track_state.h>
#include <parameters/container.h>

namespace rePlayer
{
    #define ZXtuneVersion "r5081"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::ZXTune,
        .name = "ZXTune",
        .extensions = "",
        .about = "ZXTune " ZXtuneVersion "\nCopyright(c) 2024 Vitamin/CAIG",
        .settings = "ZXTune " ZXtuneVersion,
        .init = ReplayZXTune::Init,
        .release = ReplayZXTune::Release,
        .load = ReplayZXTune::Load,
        .displaySettings = ReplayZXTune::DisplaySettings,
        .editMetadata = ReplayZXTune::Settings::Edit
    };

    bool ReplayZXTune::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayZXTuneStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayZXTuneSurround");

        Array<std::string> extensions;
        std::string buffer;
        for (auto& plugin : ZXTune::PlayerPlugin::Enumerate())
        {
            if (extensions.AddOnce(plugin->Id().data()).second)
            {
                if (!buffer.empty())
                    buffer += ';';
                for (size_t i = 0, e = plugin->Id().size(); i < e; ++i)
                    buffer += char(::tolower(plugin->Id()[i]));
            }
        }
        // add alternate extensions
        buffer += ";emul";
        // add archives extensions
        buffer += ";szx;zip;scl;hrp;fdi;bin;p;hbt;hrm;dsq;esv;tlz;tlzp;trs;cc3;gam;gamplus;lzs;msp;td0;pcd";

        auto ext = new char[buffer.size() + 1];
        memcpy(ext, buffer.c_str(), buffer.size() + 1);
        g_replayPlugin.extensions = ext;

        return false;
    }

    void ReplayZXTune::Release()
    {
        delete[] g_replayPlugin.extensions;
    }

    Replay* ReplayZXTune::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() < 8 || stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;

        struct Data : Binary::Data
        {
            Data(io::Stream* stream)
                : m_stream(stream)
            {}
            const void* Start() const override
            {
                return m_stream->Read().Items();
            }
            std::size_t Size() const override
            {
                return std::size_t(m_stream->GetSize());
            }
            SmartPtr<io::Stream> m_stream;
        };
        const Binary::Container::Ptr dataContainer = Binary::CreateContainer(Binary::Data::Ptr(new Data(stream)));

        class ModuleDetector : public Module::DetectCallback
        {
        public:
            Parameters::Container::Ptr CreateInitialProperties(StringView subpath) const override
            {
                (void)subpath;
                return Parameters::Container::Create();
            }

            void ProcessModule(const ZXTune::DataLocation& location, const ZXTune::Plugin& decoder, Module::Holder::Ptr holder) override
            {
                auto* subsong = m_subsongs.Push();
                subsong->path = location.GetPath()->AsString();
                subsong->decoderDescription = decoder.Description();

                auto chain = location.GetPluginsChain();
                std::string firstInChain;
                if (!chain->Empty())
                    firstInChain = chain->Elements()[0];

                if (firstInChain.empty())
                    subsong->type = { decoder.Id().data(), eReplay::ZXTune };
                else if (firstInChain == "ZXSTATE")
                    subsong->type.ext = eExtension::_szx;
                else if (firstInChain == "ZXZIP")
                    subsong->type.ext = eExtension::_zip;
                else if (firstInChain == "HRIP")
                    subsong->type.ext = eExtension::_hrp;
                else if (firstInChain == "HRUST1")
                    subsong->type.ext = eExtension::_bin;
                else if (firstInChain == "HRUST2")
                    subsong->type.ext = eExtension::_p;
                else if (firstInChain == "HOBETA")
                    subsong->type.ext = eExtension::_hbt;
                else if (firstInChain == "HRUM")
                    subsong->type.ext = eExtension::_hrm;
                else if (firstInChain == "TRUSH")
                    subsong->type.ext = eExtension::_trs;
                else if (firstInChain.starts_with("PCD"))
                    subsong->type.ext = eExtension::_pcd;
                else
                {
                    if (firstInChain == "RAW" && chain->Elements().size() > 1)
                    {
                        firstInChain = chain->Elements()[1];
                        if (firstInChain == "HRUST1")
                            firstInChain = "bin";
                    }
                    subsong->type = { firstInChain.c_str(), eReplay::ZXTune };
                }
                subsong->data = location.GetData();
                subsong->holder = std::move(holder);
            }
            Log::ProgressCallback* GetProgress() const override { return nullptr; }

            Array<Subsong> m_subsongs;
        };
        ModuleDetector detectCallback;
        // thx to Vitamin/CAIG for the local "static" reminder to initialise the service once and only when it's called
        static const auto service = ZXTune::Service::Create(Parameters::Container::Create());
        service->DetectModules(std::move(dataContainer), detectCallback);
        if (detectCallback.m_subsongs.IsNotEmpty())
            return new ReplayZXTune(std::move(detectCallback.m_subsongs), metadata);
        return nullptr;
    }

    bool ReplayZXTune::DisplaySettings()
    {
        // maybe one day, if I'm not lazy, I'll try to dig in ZXTune and try to add the players parameters (interpolation, chips...) (ZXTune code is so over-engineered...)
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayZXTune::Settings::Edit(ReplayMetadataContext& context)
    {
        const auto settingsSize = sizeof(Settings) + (context.lastSubsongIndex + 1) * sizeof(uint32_t);
        auto* dummy = new (_alloca(settingsSize)) Settings(context.lastSubsongIndex);
        auto* entry = context.metadata.Find<Settings>();
        if (!entry || entry->NumSubsongs() != context.lastSubsongIndex + 1u)
        {
            if (entry)
            {
                dummy->value = entry->value;
                context.metadata.Remove(entry->commandId);
            }
            entry = dummy;
        }

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        const float buttonSize = ImGui::GetFrameHeight();
        auto* durations = entry->durations;
        bool isZero = entry->value == 0;
        for (uint16_t i = 0; i <= context.lastSubsongIndex; i++)
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
                context.subsongIndex = i;
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && context.subsongIndex == i)
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

            isZero &= durations[i] == 0;
        }

        context.metadata.Update(entry, isZero);
    }

    int32_t ReplayZXTune::ms_stereoSeparation = 100;
    int32_t ReplayZXTune::ms_surround = 1;

    ReplayZXTune::~ReplayZXTune()
    {}

    ReplayZXTune::ReplayZXTune(Array<Subsong>&& subsongs, CommandBuffer metadata)
        : Replay(subsongs[0].type)
        , m_subsongs(std::move(subsongs))
        , m_renderer(m_subsongs[0].holder->CreateRenderer(kSampleRate, m_subsongs[0].holder->GetModuleProperties()))
        , m_surround(kSampleRate)
    {
        BuildDurations(metadata);
    }

    uint32_t ReplayZXTune::Render(StereoSample* output, uint32_t numSamples)
    {
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

        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_availableSamples == 0)
            {
                auto loopCount = m_renderer->GetState()->LoopCount();
                if (currentDuration == 0 && m_loopCount != loopCount)
                {
                    if (remainingSamples == numSamples)
                        m_loopCount = loopCount;
                    break;
                }

                m_chunk = m_renderer->Render();
                if (m_chunk.empty())
                    break;

                if (auto trackState = std::dynamic_pointer_cast<const Module::TrackState>(m_renderer->GetState()))
                    m_channels = Max(m_channels, trackState->Channels());
                else
                    m_channels = 2;

                m_availableSamples = uint32_t(m_chunk.size());
            }
            auto samplesToCopy = Min(remainingSamples, m_availableSamples);

            output = output->Convert(m_surround, reinterpret_cast<int16_t*>(m_chunk.end() - m_availableSamples), samplesToCopy, ms_stereoSeparation);

            m_availableSamples -= samplesToCopy;
            remainingSamples -= samplesToCopy;
        }
        if (currentDuration)
            m_currentPosition -= remainingSamples;
        return numSamples - remainingSamples;
    }

    uint32_t ReplayZXTune::Seek(uint32_t timeInMs)
    {
        m_renderer->SetPosition(Time::Instant<Time::Millisecond>(timeInMs));
        auto currentPosition = m_renderer->GetState()->At().CastTo<Time::Millisecond>().Get();
        m_currentPosition = (uint64_t(currentPosition) * kSampleRate) / 1000ull;
        m_availableSamples = 0;
        m_loopCount = 0;
        return currentPosition;
    }

    void ReplayZXTune::ResetPlayback()
    {
        m_renderer = m_subsongs[m_subsongIndex].holder->CreateRenderer(kSampleRate, m_subsongs[m_subsongIndex].holder->GetModuleProperties());
        m_availableSamples = 0;
        m_loopCount = 0;
        m_surround.Reset();
        m_currentPosition = 0;
        m_currentDuration = (uint64_t(m_subsongs[m_subsongIndex].duration) * kSampleRate) / 1000;
    }

    void ReplayZXTune::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings && settings->NumSubsongs() == GetNumSubsongs())
        {
            auto* durations = settings->durations;
            for (uint32_t i = 0, e = GetNumSubsongs(); i < e; i++)
                m_subsongs[i].duration = durations[i];
            m_currentDuration = (uint64_t(durations[m_subsongIndex]) * kSampleRate) / 1000;
        }
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
    }

    void ReplayZXTune::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayZXTune::GetDurationMs() const
    {
        return m_subsongs[m_subsongIndex].holder->GetModuleInformation()->Duration().CastTo<Time::Millisecond>().Get();
    }

    uint32_t ReplayZXTune::GetNumSubsongs() const
    {
        return m_subsongs.NumItems();
    }

    std::string ReplayZXTune::GetSubsongTitle() const
    {
        return m_subsongs[m_subsongIndex].path;
    }

    std::string ReplayZXTune::GetExtraInfo() const
    {
        std::string metadata;
        const Parameters::Accessor::Ptr props = m_subsongs[m_subsongIndex].holder->GetModuleProperties();

        String str;
        if (Parameters::FindValue(*props, Module::ATTR_TITLE, str) && !str.empty())
        {
            metadata  = "Title    : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_AUTHOR, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Artist   : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_DATE, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Date     : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_PROGRAM, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Program  : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_PLATFORM, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Platform : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_COMPUTER, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Computer : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_COMMENT, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Comment  : ";
            metadata += str;
            str.clear();
        }
        if (Parameters::FindValue(*props, Module::ATTR_STRINGS, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n\n";
            metadata += str;
        }
        return metadata;
    }

    std::string ReplayZXTune::GetInfo() const
    {
        std::string info;
        char txt[32];
        sprintf(txt, "%u channel%s", m_channels, m_channels > 1 ? "s\n" : "\n");
        info = txt;
        info += m_subsongs[m_subsongIndex].decoderDescription;
        info += "\nZXTune " ZXtuneVersion;
        return info;
    }

    void ReplayZXTune::BuildDurations(CommandBuffer metadata)
    {
        uint32_t numSubsongs = GetNumSubsongs();
        auto settings = metadata.Find<Settings>();
        if (settings && settings->NumSubsongs() == numSubsongs)
        {
            for (uint32_t i = 0; i < numSubsongs; i++)
                m_subsongs[i].duration = settings->durations[i];
        }
        else
        {
            metadata.Remove(Settings::kCommandId);
            for (uint16_t i = 0; i < numSubsongs; i++)
                m_subsongs[i].duration = 0;
        }
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer