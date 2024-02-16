#include "ReplayProTrekkr.h"
#include <include/version.h>
#include <replay/include/replay.h>
#include <files/include/files.h>

#include <Core/String.h>
#include <ReplayDll.h>

int AUDIO_Play_Flag;

char artist[20];
char style[20];
char SampleName[128][16][64];
int Midiprg[128];
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
        .replayId = eReplay::ProTrekkr, .isThreadSafe = false,
        .name = TITLE,
        .extensions = "ptk",
        .about = TITLE " " VER_VER "." VER_REV "." VER_REVSMALL "\nCopyright (c) 2008-2024 Franck Charlet",
        .load = ReplayProTrekkr::Load
    };

    Replay* ReplayProTrekkr::Load(io::Stream* stream, CommandBuffer metadata)
    {
        (void)metadata;
        auto data = stream->Read();
        if (data.Size() < 8 || memcmp(data.Items(), "PROTREK", 7) != 0)
            return nullptr;

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