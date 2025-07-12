#include "ReplayMegaTracker.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

extern "C"
{
#   include "mtpng/ntgs.h"
#   include "mtpng/oss.h"
#   include "mtpng/smus.h"
#   include "mtpng/ssmt.h"

    static MTPInputT* input_plugins[] =
    {
        &ssmt_plugin,		// soundsmith/megatracker
        &ntgs_plugin,		// noisetracker GS
        &smus_plugin,		// will harvey SMUS
        (MTPInputT*)NULL
    };

    int frequency = 48000;
    extern unsigned long totalsamples;
    extern uint32 SPT;

    // file.c wrapper
    static core::SmartPtr<core::io::Stream> s_mainFile;
    static core::SmartPtr<core::io::Stream> s_file;

    int8 FILE_Open(char* fileName)
    {
        if (_stricmp(s_mainFile->GetName().c_str(), fileName) != 0)
        {
            s_file = s_mainFile->Open(fileName);
            if (s_file.IsInvalid())
                return 0;
        }
        else
		{
			s_file = s_mainFile;
            s_file->Seek(0, core::io::Stream::kSeekBegin);
		}

        return 1;
    }

    uint32 FILE_GetLength(void)
    {
        if (!s_file)
            return 0;

        return uint32(s_file->GetSize());
    }

    void FILE_Seek(uint32 position)
    {
        if (!s_file)
            return;

        s_file->Seek(position, core::io::Stream::kSeekBegin);
    }

    void FILE_Seekrel(uint32 position)
    {
        if (!s_file)
            return;

        s_file->Seek(position, core::io::Stream::kSeekCurrent);
    }

    uint32 FILE_GetPos(void)
    {
        return uint32(s_file->GetPosition());
    }

    uint32 FILE_Read(uint32 length, uint8* where)
    {
        if (!s_file)
            return 0;

        return uint32(s_file->Read(where, length));
    }

    uint8 FILE_Readbyte(void)
    {
        uint8 inbyte;

        if (!s_file)
            return 0;

        s_file->Read(&inbyte, 1);

        return inbyte;
    }

    uint16 FILE_Readword(void)
    {
        uint16 inword;

        s_file->Read(&inword, 2);

        return inword;
    }

    uint32 FILE_Readlong(void)
    {
        uint32 inlong;

        s_file->Read(&inlong, 4);

        return inlong;
    }

    void FILE_Close(void)
    {
        s_file.Reset();
    }

    int8 FILE_FindCaseInsensitive(int8* inpath, int8* file, int8* outfile)
    {
        std::string filename = (const char*)inpath;
        filename += (const char*)file;
        auto newFile = s_mainFile->Open(filename);
        strcpy_s((char*)outfile, 32, (const char*)file);
        return newFile.IsValid();
    }
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::MegaTracker, .isThreadSafe = false,
        .name = "MegaTracker Player",
        .extensions = "",
        .about = "MegaTracker Player 4.3.1\nCopyright (c) 1991-2000 Ian Schmidt",
        .settings = "MegaTracker Player 4.3.1",
        .init = ReplayMegaTracker::Init,
        .load = ReplayMegaTracker::Load,
        .displaySettings = ReplayMegaTracker::DisplaySettings,
        .editMetadata = ReplayMegaTracker::Settings::Edit,
        .globals = &ReplayMegaTracker::ms_globals
    };

    bool ReplayMegaTracker::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplayMegaTrackerStereoSeparation");
            window.RegisterSerializedData(ms_globals.surround, "ReplayMegaTrackerSurround");
            window.RegisterSerializedData(ms_globals.oldStyle, "ReplayMegaTrackerOldStyle");
        }

        return false;
    }

    Replay* ReplayMegaTracker::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        s_mainFile = stream;

        int ftype = 0;
        while (input_plugins[ftype] != (MTPInputT*)NULL)
        {
            if (input_plugins[ftype]->IsFile((char*)stream->GetName().c_str()))
                return new ReplayMegaTracker(input_plugins[ftype]);
            else
                ftype++;
        }

        return nullptr;
    }

    bool ReplayMegaTracker::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, NumItemsOf(surround));
        const char* const oldStyle[] = { "New", "Old" };
        changed |= ImGui::Combo("Pitch Slides", &ms_globals.oldStyle, oldStyle, NumItemsOf(oldStyle));
        return changed;
    }

    void ReplayMegaTracker::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");
        ComboOverride("OldStyle", GETSET(entry, overrideOldStyle), GETSET(entry, oldStyle),
            ms_globals.oldStyle, "New Pitch Slides", "Old Pitch Slides");

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplayMegaTracker::Globals ReplayMegaTracker::ms_globals = {
        .stereoSeparation = 100,
        .surround = 1
    };

    ReplayMegaTracker::~ReplayMegaTracker()
    {
        GUS_Exit();
        m_plugin->Close();
    }

    uint32_t ReplayMegaTracker::GetSampleRate() const
    {
        return frequency;
    }

    ReplayMegaTracker::ReplayMegaTracker(MTPInputT* plugin)
        : Replay(plugin != &ntgs_plugin ? eExtension::_ : eExtension::_ntg, eReplay::MegaTracker)
        , m_surround(frequency)
        , m_plugin(plugin)
    {
        GUS_Init();

        plugin->LoadFile((char*)s_mainFile->GetName().c_str());

        ResetPlayback();
    }

    uint32_t ReplayMegaTracker::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_numSamples == 0 && !m_plugin->GetSongStat())
        {
            m_plugin->ResetSongStat();
            return 0;
        }

        auto remainingSamples = numSamples;
        while (remainingSamples > 0)
        {
            if (m_numSamples == 0)
			{
                if (!m_plugin->GetSongStat())
                    break;

				GUS_Update();

                m_numSamples = SPT;
			}

            auto samplesToRender = Min(m_numSamples, remainingSamples);
            output = output->Convert(m_surround, GUS_GetSamples() + (SPT - m_numSamples) * 2, samplesToRender, m_stereoSeparation);
            remainingSamples -= samplesToRender;
            m_numSamples -= samplesToRender;
        }

        return numSamples - remainingSamples;
    }

    void ReplayMegaTracker::ResetPlayback()
    {
        m_surround.Reset();

        m_plugin->SetOldstyle(m_oldStyle);
        m_plugin->SetScroll(0);
        m_plugin->PlayStart();
        GUS_PlayStart();

        m_numSamples = 0;
    }

    void ReplayMegaTracker::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globals->stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);
        m_oldStyle = (settings && settings->overrideOldStyle) ? settings->oldStyle : globals->oldStyle;

        m_plugin->SetOldstyle(m_oldStyle);
    }

    void ReplayMegaTracker::SetSubsong(uint32_t subsongIndex)
    {
        (void)subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayMegaTracker::GetDurationMs() const
    {
        while (m_plugin->GetSongStat())
            GUS_UpdateFake();

        return uint32_t((totalsamples * 1000ull) / frequency);
    }

    uint32_t ReplayMegaTracker::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayMegaTracker::GetExtraInfo() const
    {
        return {};
    }

    std::string ReplayMegaTracker::GetInfo() const
    {
        std::string info;

        char buf[8];
        sprintf(buf, "%d", m_plugin->GetNumChannels());
        info = buf;
        info += " channels\n";
        if (m_plugin == &ssmt_plugin)
            info += "SoundSmith";
        else if (m_plugin == &ntgs_plugin)
            info += "NoiseTracker GS";
        else
            info += "Will Harvey SMUS";
        info += "\nMegaTracker Player 4.3.1";
        return info;
    }
}
// namespace rePlayer