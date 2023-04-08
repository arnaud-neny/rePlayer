#include "ReplayProTrekkr.h"
#include <include/version.h>
#include <replay/include/replay.h>
#include <files/include/files.h>

#include "dllloader/dllloader.h"

#include <Core/String.h>
#include <IO/File.h>
#include <ReplayDll.h>

#include <filesystem>

int AUDIO_Play_Flag;

char artist[20];
char style[20];
char SampleName[128][16][64];
char Midiprg[128];
char nameins[128][20];

int Chan_Midi_Prg[MAX_TRACKS];
char Chan_History_State[256][MAX_TRACKS];

int done = 0; // set to TRUE if decoding has finished

extern Uint8* Mod_Memory;
extern char replayerPtkName[20];
int Calc_Length(void);
Uint32 STDCALL Mixer(Uint8* Buffer, Uint32 Len);
int Alloc_Patterns_Pool(void);

// ------------------------------------------------------
// Reset the channels polyphonies  to their default state
void Set_Default_Channels_Polyphony(void)
{
    int i;

    for (i = 0; i < MAX_TRACKS; i++)
    {
        Channels_Polyphony[i] = DEFAULT_POLYPHONY;
    }
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::ProTrekkr,
        .name = TITLE,
        .extensions = "ptk",
        .about = TITLE " " VER_VER "." VER_REV "." VER_REVSMALL "\nCopyright (c) 2008-2022 Franck Charlet",
        .init = ReplayProTrekkr::Init,
        .release = ReplayProTrekkr::Release,
        .load = ReplayProTrekkr::Load
    };

    // Dll hook begin
    static DllManager* s_dllManager = nullptr;
    static bool s_isMainModule = false;
    struct DllEntry
    {
        std::wstring path;
        HMODULE handle;
        ReplayProTrekkr* replay;
    };
    static Array<DllEntry> s_dlls;
    SharedContexts* s_sharedContexts = nullptr;

    typedef ReplayPlugin* (*GetReplayPlugin)();
    // Dll hook end

    // todo remove?
    bool ReplayProTrekkr::Init(SharedContexts* ctx, Window& window)
    {
        s_sharedContexts = ctx;
        ctx->Init();

        if (&window != nullptr)
        {
            s_isMainModule = true;
            s_dlls.Reserve(8);
        }

        return false;
    }

    void ReplayProTrekkr::Release()
    {
        for (auto& dllEntry : s_dlls)
        {
            ::FreeLibrary(dllEntry.handle);
            s_dllManager->UnsetDllFile(dllEntry.path.c_str());
        }
        s_dlls = {};
        delete s_dllManager;
    }

    Replay* ReplayProTrekkr::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();
        if (memcmp(data.Items(), "PROTREK", 7) != 0)
            return nullptr;

        if (s_isMainModule)
        {
            // ProTrekker is a hard mess... to make it "thread safe" without rewriting everything, we duplicate the dll per song so all the data are in the module memory space.
            // Thanx to dll manager for that.
            // We still have to share some data from the main dll (replayProTrekkr.dll) with the others such as the SharedContext...

            // Load the main dll in memory
            char* pgrPath;
            _get_pgmptr(&pgrPath);
            auto mainPath = std::filesystem::path(pgrPath).remove_filename() / "replays/ProTrekkr.dll";
            mainPath = mainPath.lexically_normal(); // important for the dll loader

            Array<uint8_t> b;
            {
                auto f = io::File::OpenForRead(mainPath.c_str());
                b.Resize(f.GetSize());
                f.Read(b.Items(), b.Size());
            }

            // Create a unique name for it
            static uint32_t counter = 0;
            char dllName[32];
            sprintf(dllName, "ProTrekkr%08X.dll", counter++);

            // and load it through the dll manager (created only on the first load)
            auto freeIndex = s_dlls.NumItems();
            if (s_dllManager == nullptr)
            {
                s_dllManager = new DllManager();
                s_dllManager->EnableDllRedirection();
            }
            else for (uint32_t i = 0; i < s_dlls.NumItems(); i++)
            {
                auto& dllEntry = s_dlls[i];
                if (dllEntry.handle != nullptr)
                {
                    if (dllEntry.replay == nullptr)
                    {
                        ::FreeLibrary(dllEntry.handle);
                        s_dllManager->UnsetDllFile(dllEntry.path.c_str());
                        dllEntry.handle = nullptr;
                        freeIndex = i;
                    }
                }
                else
                    freeIndex = i;
            }

            mainPath.replace_filename(dllName);
            s_dllManager->SetDllFile(mainPath.c_str(), b.Items(), b.Size());

            auto dllHandle = s_dllManager->LoadLibrary(mainPath.c_str());
            ReplayProTrekkr* replay = nullptr;
            if (dllHandle != 0)
            {
                // load the song though the new module
                auto g = reinterpret_cast<GetReplayPlugin>(GetProcAddress(dllHandle, "getReplayPlugin"));
                Window* w = nullptr;
                g()->init(s_sharedContexts, reinterpret_cast<Window&>(*w));
                replay = reinterpret_cast<ReplayProTrekkr*>(g()->load(stream, metadata));
                if (replay)
                {
                    replay->m_dllIndex = freeIndex;
                    replay->m_dllEntries = &s_dlls;
                    if (freeIndex == s_dlls.NumItems())
                        s_dlls.Add({ std::move(mainPath), dllHandle, replay });
                    else
                        s_dlls[freeIndex] = { std::move(mainPath), dllHandle, replay };
                }
                else
                {
                    ::FreeLibrary(dllHandle);
                    s_dllManager->UnsetDllFile(mainPath.c_str());
                }
            }
            else
            {
                auto s = s_dllManager->GetLastError();
                s.clear();
            }

            return replay;
        }
        Ptk_InitDriver();
        Alloc_Patterns_Pool();

        if (!Load_Ptk({ data.Items(), data.Size() }))
            return nullptr;

        return new ReplayProTrekkr(stream);
    }

    ReplayProTrekkr::~ReplayProTrekkr()
    {
        Ptk_Stop();
        Free_Samples();
        if (Mod_Memory) free(Mod_Memory);
        if (RawPatterns) free(RawPatterns);
        if (m_dllEntries)
            (*m_dllEntries)[m_dllIndex].replay = nullptr;
    }

    ReplayProTrekkr::ReplayProTrekkr(io::Stream* stream)
        : Replay(eExtension::_ptk, eReplay::ProTrekkr)
        , m_stream(stream)
        , m_duration(uint32_t(Calc_Length()))
    {
        Ptk_Play();
    }

    uint32_t ReplayProTrekkr::GetSampleRate() const
    {
        return MIX_RATE;
    }

    uint32_t ReplayProTrekkr::Render(StereoSample* output, uint32_t numSamples)
    {
        return Mixer((BYTE*)output, numSamples);
    }

    void ReplayProTrekkr::ResetPlayback()
    {
        Ptk_Stop();
        if (Mod_Memory) free(Mod_Memory);
        auto data = m_stream->Read();
        Load_Ptk({ data.Items(), data.Size() });
        Ptk_Play();
    }

    void ReplayProTrekkr::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayProTrekkr::SetSubsong(uint16_t /*subsongIndex*/)
    {}

    uint32_t ReplayProTrekkr::GetDurationMs() const
    {
        return m_duration;
    }

    uint32_t ReplayProTrekkr::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayProTrekkr::GetExtraInfo() const
    {
        std::string info;
        info = replayerPtkName;
        if (replayerPtkName[0])
            info += "\n";
        info += artist;
        if (artist[0])
            info += "\n";
        info += style;
        return info;
    }

    std::string ReplayProTrekkr::GetInfo() const
    {
        std::string info;
        char buf[32];
        sprintf(buf, "%d channels\n", int(Songtracks));
        info = buf;
        info += m_stream->Read().Items<const char>();
        info += "\n" TITLE " " VER_VER "." VER_REV "." VER_REVSMALL;
        return info;
    }
}
// namespace rePlayer