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
    #define ZXtuneVersion "r5050"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::ZXTune,
        .name = "ZXTune",
        .extensions = "",
        .about = "ZXTune " ZXtuneVersion "\nCopyright(c) 2023 Vitamin/CAIG",
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
            if (extensions.AddOnce(plugin->Id().to_string()).second)
            {
                if (!buffer.empty())
                    buffer += ';';
                for (size_t i = 0, e = plugin->Id().size(); i < e; ++i)
                    buffer += char(::tolower(plugin->Id()[i]));
            }
        }
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
        (void)metadata;
        auto data = stream->Read();
        const Binary::Container::Ptr dataContainer = Binary::CreateContainer({ data.Items(), data.Size() });

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
                    firstInChain = chain->GetIterator()->Get();

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
                    if (firstInChain == "RAW")
                    {
                        auto chainIt = chain->GetIterator();
                        chainIt->Next();
                        firstInChain = chainIt->Get();
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
        auto service = ZXTune::Service::Create(Parameters::Container::Create());
        service->DetectModules(std::move(dataContainer), detectCallback);
        if (detectCallback.m_subsongs.IsNotEmpty())
            return new ReplayZXTune(std::move(detectCallback.m_subsongs));
        return nullptr;
    }

    bool ReplayZXTune::DisplaySettings()
    {
        // maybe one day, if I'm not lazy, I'll try to dig in ZXTune and try to add the players parameters (interpolation, chips...) (ZXTune code is so over-engineered...)
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplayZXTune::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayZXTune::ms_stereoSeparation = 100;
    int32_t ReplayZXTune::ms_surround = 1;

    ReplayZXTune::~ReplayZXTune()
    {}

    ReplayZXTune::ReplayZXTune(Array<Subsong>&& subsongs)
        : Replay(subsongs[0].type)
        , m_subsongs(std::move(subsongs))
        , m_renderer(m_subsongs[0].holder->CreateRenderer(kSampleRate, m_subsongs[0].holder->GetModuleProperties()))
        , m_surround(kSampleRate)
    {}

    uint32_t ReplayZXTune::Render(StereoSample* output, uint32_t numSamples)
    {
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_availableSamples == 0)
            {
                auto loopCount = m_renderer->GetState()->LoopCount();
                if (m_loopCount != loopCount)
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

        return numSamples - remainingSamples;
    }

    uint32_t ReplayZXTune::Seek(uint32_t timeInMs)
    {
        m_renderer->SetPosition(Time::Instant<Time::Millisecond>(timeInMs));
        return m_renderer->GetState()->At().CastTo<Time::Millisecond>().Get();
    }

    void ReplayZXTune::ResetPlayback()
    {
        m_renderer = m_subsongs[m_subsongIndex].holder->CreateRenderer(kSampleRate, m_subsongs[m_subsongIndex].holder->GetModuleProperties());
        m_availableSamples = 0;
        m_surround.Reset();
    }

    void ReplayZXTune::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
    }

    void ReplayZXTune::SetSubsong(uint16_t subsongIndex)
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
        if (props->FindValue(Module::ATTR_TITLE, str) && !str.empty())
        {
            metadata  = "Title    : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_AUTHOR, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Artist   : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_DATE, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Date     : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_PROGRAM, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Program  : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_PLATFORM, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Platform : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_COMPUTER, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Computer : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_COMMENT, str) && !str.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Comment  : ";
            metadata += str;
            str.clear();
        }
        if (props->FindValue(Module::ATTR_STRINGS, str) && !str.empty())
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
}
// namespace rePlayer