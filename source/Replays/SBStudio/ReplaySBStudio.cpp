#include "ReplaySBStudio.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

core::io::Stream* theStream = nullptr;

extern "C" {
    // wrap stdio
    void* replayer_fopen(const char* filename, const char* mode)
    {
        core::UnusedArg(filename, mode);
        theStream->Seek(0, core::io::Stream::kSeekBegin);
        return theStream;
    }
    int replayer_fread(void* buffer, size_t size, size_t count, void* out)
    {
        core::UnusedArg(out);
        return int(theStream->Read(buffer, size * count) / size);
    }
    int replayer_fseek(FILE* stream, long offset, int origin)
    {
        core::UnusedArg(stream);
        return int(theStream->Seek(offset, core::io::Stream::SeekWhence(origin)));
    }
    int replayer_fclose(void* out)
    {
        core::UnusedArg(out);
        return 0;
    }
    int replayer_ftell(void* stream)
    {
        core::UnusedArg(stream);
        return int(theStream->GetPosition());
    }
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SBStudio, .isThreadSafe = false,
        .name = "SBStudio",
        .extensions = "pac",
        .about = "libpac " PAC_VERSION "\nCopyright (c) Thomas Pfaff",
        .settings = "SBStudio",
        .load = ReplaySBStudio::Load,
        .displaySettings = ReplaySBStudio::DisplaySettings,
        .editMetadata = ReplaySBStudio::Settings::Edit,
        .globals = &ReplaySBStudio::ms_globals
    };

    bool ReplaySBStudio::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplaySBStudioStereoSeparation");
            window.RegisterSerializedData(ms_globals.surround, "ReplaySBStudioSurround");
        }

        return false;
    }

    Replay* ReplaySBStudio::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        pac_init(kSampleRate, 32, 2);

        theStream = stream;
        if (auto* m = pac_open(stream->GetName().c_str()))
            return new ReplaySBStudio(m);

        pac_exit();
        return nullptr;
    }

    bool ReplaySBStudio::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplaySBStudio::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplaySBStudio::Globals ReplaySBStudio::ms_globals = {
        .stereoSeparation = 100,
        .surround = 1
    };

    ReplaySBStudio::~ReplaySBStudio()
    {
        pac_close(m_module);
        pac_exit();
    }

    ReplaySBStudio::ReplaySBStudio(pac_module* module)
        : Replay(eExtension::_pac, eReplay::SBStudio)
        , m_surround(kSampleRate)
        , m_module(module)
    {
        // push empty property info
        auto* property = m_properties.Push();
        property->label = "Info";
        property->numColumns = 2;
        {
            property->Add("Title", Property::kIsNotEditable, m_module->name, Property::kIsEditable);

            char buf[16];
            sprintf(buf, "%d", m_module->channelcnt);
            property->Add("Channels", Property::kIsNotEditable, buf, Property::kIsNotEditable);
            sprintf(buf, "%d", m_module->soundcnt);
            property->Add("Sounds", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        }
        // push property instruments if any
        if (m_module->soundcnt)
        {
            property = m_properties.Push();
            property->label = "Sounds";
            property->numColumns = 1;
            for (int i = 0; i < m_module->soundcnt; ++i)
                property->Add(m_module->sound[i].name, Property::kIsEditable);
        }
    }

    uint32_t ReplaySBStudio::Render(StereoSample* output, uint32_t numSamples)
    {
        auto n = pac_read(m_module, output, numSamples * sizeof(StereoSample)) / sizeof(StereoSample);
        if (n == 0)
        {
            m_module->eof = 0;
            m_module->nextpos = 0;
            m_module->time = 0;
        }
        else
        {
            auto surround = m_surround.Begin();
            float stereo = 0.5f - 0.5f * m_stereoSeparation / 100.0f;

            for (long i = 0; i < n; ++i)
            {
                auto left = pcCast<int>(output)[0] / 32767.0f;
                auto right = pcCast<int>(output)[1] / 32767.0f;

                StereoSample s;
                s.left = left + (right - left) * stereo;
                s.right = right + (left - right) * stereo;
                *output++ = surround(s);
            }

            m_surround.End(surround);
        }
        return uint32_t(n);
    }

    uint32_t ReplaySBStudio::Seek(uint32_t timeInMs)
    {
        auto time = (uint64_t(timeInMs) * kSampleRate) / 1000ull;
        return uint32_t((pac_seek(m_module, long(time), SEEK_SET) * 1000Ull) / kSampleRate);
    }

    void ReplaySBStudio::ResetPlayback()
    {
        pac_seek(m_module, 0, SEEK_SET);
        m_surround.Reset();
    }

    void ReplaySBStudio::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globals->stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);
    }

    void ReplaySBStudio::SetSubsong(uint32_t subsongIndex)
    {
        UnusedArg(subsongIndex);
    }

    uint32_t ReplaySBStudio::GetDurationMs() const
    {
        return uint32_t(pac_length(m_module) * 1000ull / kSampleRate);
    }

    uint32_t ReplaySBStudio::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplaySBStudio::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_module->name;
        metadata += "\n";
        for (int i = 0; i < m_module->soundcnt; ++i)
        {
            metadata += "\n";
            metadata += m_module->sound[i].name;
        }
        return metadata;
    }

    std::string ReplaySBStudio::GetInfo() const
    {
        std::string info;
        char buf[8];
        sprintf(buf, "%d", m_module->channelcnt);
        info = buf;
        info += " channels\nSBStudio\nlibpac " PAC_VERSION;
        return info;
    }

    const Replay::Properties& ReplaySBStudio::BuildProperties()
    {
        return m_properties;
    }
}
// namespace rePlayer