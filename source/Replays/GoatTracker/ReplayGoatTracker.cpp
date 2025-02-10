#include "ReplayGoatTracker.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    #define GoatTrackerVersion "2.77"

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::GoatTracker, .isThreadSafe = false,
        .name = "GoatTracker",
        .extensions = "sng",
        .about = "GoatTracker" GoatTrackerVersion "\nCopyright(c) 2025 Lasse Öörni",
        .settings = "GoatTracker" GoatTrackerVersion,
        .init = ReplayGoatTracker::Init,
        .load = ReplayGoatTracker::Load,
        .displaySettings = ReplayGoatTracker::DisplaySettings,
        .editMetadata = ReplayGoatTracker::Settings::Edit,
        .globals = &ReplayGoatTracker::ms_globals
    };

    static SmartPtr<io::Stream> sMainStream;

    extern "C"
    {
        typedef void FILE;
#       define STDIO_WRAP_H
#       include "src/goattrk2.h"

        // goattrk2
        int editmode = EDIT_PATTERN;
        int recordmode = 0;
        int followplay = 0;
        int hexnybble = -1;
        int stepsize = 4;
        int autoadvance = 0;
        int defaultpatternlength = 64;
        int exitprogram = 0;
        unsigned keypreset = KEY_TRACKER;
        unsigned playerversion = 0;
        int fileformat = FORMAT_PRG;
        int zeropageadr = 0xfc;
        int playeradr = 0x1000;
        unsigned sidmodel = 0;
        unsigned multiplier = 1;
        unsigned adparam = 0x0f00;
        unsigned ntsc = 0;
        unsigned sidaddress = 0xd400;
        unsigned finevibrato = 1;
        unsigned optimizepulse = 1;
        unsigned optimizerealtime = 1;
        unsigned interpolate = 0;
        unsigned residdelay = 0;

        char loadedsongfilename[MAX_FILENAME] = { 0 };
        char songfilename[MAX_FILENAME] = "song.sng";
        char instrfilename[MAX_FILENAME];
        char packedpath[MAX_PATHNAME];

        char* programname;

        char textbuffer[MAX_PATHNAME];

        // stdio_wrap
        FILE* replayer_fopen(const char* filename, const char* mode)
        {
            FILE* f;
            assert(strcmp(mode, "rb") == 0);
            UnusedArg(mode);

            if (strcmp(filename, "song.sng") == 0)
                f = reinterpret_cast<FILE*>(sMainStream->Clone().Detach());
            else
                f = reinterpret_cast<FILE*>(sMainStream->Open(filename).Detach());
            return f;
        }
        int replayer_fread(void* buffer, size_t size, size_t count, FILE* stream)
        {
            return int(reinterpret_cast<io::Stream*>(stream)->Read(buffer, size * count));
        }
        int replayer_fwrite(void* buffer, size_t size, size_t count, FILE* stream)
        {
            assert(0);
            UnusedArg(buffer, size, count, stream); return 0;
        }
        int replayer_fseek(FILE* stream, long offset, int origin)
        {
            return int(reinterpret_cast<io::Stream*>(stream)->Seek(offset, io::Stream::SeekWhence(origin)));
        }
        void replayer_fclose(FILE* stream)
        {
            reinterpret_cast<io::Stream*>(stream)->Release();
        }
        int replayer_fprintf(void* const, char const* const, ...) { return 0; }

        // bme_end
        void fwrite8(FILE* file, unsigned data) { core::UnusedArg(file, data); }
        void fwritele16(FILE* file, unsigned data) { core::UnusedArg(file, data); }
        unsigned fread8(FILE* file) { uint8_t buf = 0; replayer_fread(&buf, 1, 1, file); return buf; }

        // bme_io
        int io_open(char* name) { core::UnusedArg(name); return 0; }
        int io_lseek(int handle, int bytes, int whence) { core::UnusedArg(handle, bytes, whence); return 0; }
        void io_close(int handle) { core::UnusedArg(handle); }
        unsigned io_read8(int handle) { core::UnusedArg(handle); return 0; }

        // bme_win
        int win_quitted = 0;

        // gconsole
        int key = 0;
        int rawkey = 0;
        int shiftpressed = 0;

        void clearscreen(void) {}
        void fliptoscreen(void) {}
        void printtext(int x, int y, int color, const char* text) { core::UnusedArg(x, y, color, text); }
        void printtextc(int y, int color, const char* text) { core::UnusedArg(y, color, text); }
        void printblankc(int x, int y, int color, int length) { core::UnusedArg(x, y, color, length); }

        // gdisplay
        void printmainscreen(void) {}
        void resettime(void) {}
        void incrementtime(void) {}

        // gfile
        int fileselector(char* name, char* path, char* filter, char* title, int filemode) { core::UnusedArg(name, path, filter, title, filemode); return 0; }
        void editstring(char* buffer, int maxlength) { core::UnusedArg(buffer, maxlength); }

        // gsound
        void sound_suspend(void) {}
        void sound_flush(void) {}

        //
        void waitkeynoupdate(void) {}
    }
    int stereosid = 1;

    bool ReplayGoatTracker::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.isSidModel8580, "ReplayGoatTrackerSidModel");
            window.RegisterSerializedData(ms_globals.isNtsc, "ReplayGoatTrackerNtsc");
            window.RegisterSerializedData(ms_globals.isFiltering, "ReplayGoatTrackerFiltering");
            window.RegisterSerializedData(ms_globals.isDistortion, "ReplayGoatTrackerDistortion");
            window.RegisterSerializedData(ms_globals.surround, "ReplayGoatTrackerSurround");
        }

        return false;
    }

    Replay* ReplayGoatTracker::Load(io::Stream* stream, CommandBuffer metadata)
    {
        initchannels();
        clearsong(1, 1, 1, 1, 1);

        sMainStream = stream;
        loadsong();

        if (strcmp(loadedsongfilename, songfilename) != 0)
            return nullptr;

        uint32_t songs = 0;
        // Calculate amount of songs with nonzero length
        for (int c = 0; c < MAX_SONGS; c++)
        {
            if ((songlen[c][0]) &&
                (songlen[c][1]) &&
                (songlen[c][2]))
                songs++;
        }

        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto* settings = metadata.Find<Settings>();

        bool isNtsc = (settings && settings->overrideNtsc) ? settings->isNtsc : globals->isNtsc;
        sid_init(kSampleRate
            , (settings && settings->overrideSidModel) ? settings->isSidModel8580 : globals->isSidModel8580
            , isNtsc
            , (settings && settings->overrideFiltering) ? settings->isFiltering : globals->isFiltering
            , 0
            , (settings && settings->overrideDistortion) ? settings->isDistortion : globals->isDistortion);
        for (uint32_t i = 0; i < 16384; ++i)
            playroutine();

        initsong(esnum, PLAY_BEGINNING);

        stream->Seek(0, io::Stream::kSeekBegin);
        uint32_t songId;
        stream->Read(&songId, sizeof(songId));
        return new ReplayGoatTracker(songs, songId, isNtsc);
    }

    bool ReplayGoatTracker::DisplaySettings()
    {
        bool changed = false;
        {
            const char* const sidModels[] = { "6581", "8580" };
            int index = ms_globals.isSidModel8580 ? 1 : 0;
            changed |= ImGui::Combo("SID Default Model", &index, sidModels, NumItemsOf(sidModels));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
            ms_globals.isSidModel8580 = index == 1;
        }
        {
            const char* const clocks[] = { "PAL", "NTSC" };
            auto index = ms_globals.isNtsc ? 1 : 0;
            changed |= ImGui::Combo("Default Clock###SidClock", &index, clocks, NumItemsOf(clocks));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
            ms_globals.isNtsc = index == 1;
        }
        {
            const char* const filters[] = { "off", "on" };
            auto index = ms_globals.isFiltering ? 1 : 0;
            changed |= ImGui::Combo("Filtering", &index, filters, NumItemsOf(filters));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
            ms_globals.isFiltering = index == 1;
        }
        {
            const char* const distortions[] = { "off", "on" };
            auto index = ms_globals.isDistortion ? 1 : 0;
            changed |= ImGui::Combo("Distortion", &index, distortions, NumItemsOf(distortions));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
            ms_globals.isDistortion = index == 1;
        }
        {
            const char* const surround[] = { "Default", "Surround" };
            changed |= ImGui::Combo("Output", &ms_globals.surround, surround, NumItemsOf(surround));
        }
        return changed;
    }

    void ReplayGoatTracker::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("SidModel", GETSET(entry, overrideSidModel), GETSET(entry, isSidModel8580),
            ms_globals.isSidModel8580, "Sid Model: 6581", "Sid Model: 8580");
        ComboOverride("SidClock", GETSET(entry, overrideNtsc), GETSET(entry, isNtsc),
            ms_globals.isNtsc, "Clock Speed: PAL", "Clock Speed: NTSC");
        ComboOverride("Filtering", GETSET(entry, overrideFiltering), GETSET(entry, isFiltering),
            ms_globals.isFiltering, "Filtering: off", "Filtering: on");
        ComboOverride("Distortion", GETSET(entry, overrideDistortion), GETSET(entry, isDistortion),
            ms_globals.isDistortion, "Distortion: off", "Distortion: on");
        ComboOverride("Output", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Default", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplayGoatTracker::Globals ReplayGoatTracker::ms_globals = {
        .isSidModel8580 = 0,
        .isNtsc = 0,
        .isFiltering = 0,
        .isDistortion = 0,
        .surround = 1
    };

    ReplayGoatTracker::~ReplayGoatTracker()
    {
        sid_uninit();
    }

    ReplayGoatTracker::ReplayGoatTracker(uint32_t numSongs, uint32_t songId, bool isNtsc)
        : Replay(eExtension::_sng, eReplay::GoatTracker)
        , m_surround(kSampleRate)
        , m_numSongs(numSongs)
        , m_songId(songId)
        , m_tempo(isNtsc ? 150 : 125)
    {}

    struct Sequence
    {
        uint32_t songsPtr;
        uint32_t pattPtr[MAX_CHN];
    };

    uint32_t ReplayGoatTracker::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }

        auto* buffer = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;

        uint32_t mixSamples = numSamples;

        stereosid = m_surround.IsEnabled();
        while (mixSamples)
        {
            if (!m_remainingSamples)
            {
                auto getPatBreak = []()
                {
                    for (uint32_t i = 0; i < MAX_CHN; ++i)
                    {
                        if (chn[i].repeat != 0)
                            return false;
                        if (chn[i].pattptr == 0x7fFFffFF)
                            return true;
                    }
                    return false;
                };
                bool hasPatBreak = getPatBreak();

                playroutine();

                m_remainingSamples = ((kSampleRate * 5) >> 1) / m_tempo;

                if (hasPatBreak && !getPatBreak())
                {
                    uint32_t songsPtr = 0;
                    for (uint32_t i = 0; i < MAX_CHN; ++i)
                        songsPtr |= uint32_t(chn[i].songptr) << (i * 8);
                    if (m_sequence.FindIf([songsPtr](auto& entry)
                    {
                        if (entry.songsPtr == songsPtr)
                        {
                            for (uint32_t i = 0; i < MAX_CHN; ++i)
                            {
                                if (entry.pattPtr[i] != chn[i].pattptr)
                                    return false;
                            }
                            return true;
                        }
                        return false;
                    }))
                    {
                        m_sequence.Clear();
                        auto* sequence = m_sequence.Push();
                        sequence->songsPtr = songsPtr;
                        for (uint32_t i = 0; i < MAX_CHN; ++i)
                            sequence->pattPtr[i] = chn[i].pattptr;
                        m_hasLooped = numSamples != mixSamples;
                        return numSamples - mixSamples;
                    }

                    auto* sequence = m_sequence.Push();
                    sequence->songsPtr = songsPtr;
                    for (uint32_t i = 0; i < MAX_CHN; ++i)
                        sequence->pattPtr[i] = chn[i].pattptr;
                }
            }

            uint32_t samplesToRender = mixSamples;
            if (samplesToRender > m_remainingSamples)
                samplesToRender = m_remainingSamples;

            sid_fillbuffer(buffer + (numSamples - mixSamples) * 2, samplesToRender);

            m_remainingSamples -= samplesToRender;
            mixSamples -= samplesToRender;
        }
        output->Convert(m_surround, buffer, numSamples, 100, m_surround.IsEnabled() ? 1.33f : 1.0f);
        return numSamples;
    }

    void ReplayGoatTracker::ResetPlayback()
    {
        stopsong();
        for (uint32_t i = 0; i < 16384; ++i)
            playroutine();

        int subsongIndex = 0;
        for (uint32_t songs = 0; subsongIndex < MAX_SONGS; subsongIndex++)
        {
            if ((songlen[subsongIndex][0]) &&
                (songlen[subsongIndex][1]) &&
                (songlen[subsongIndex][2]))
            {
                if (songs == m_subsongIndex)
                    break;
                songs++;
            }
        }
        initsong(subsongIndex, PLAY_BEGINNING);
        m_remainingSamples = 0;
        m_hasLooped = false;
        m_sequence.Clear();

        m_surround.Reset();
    }

    void ReplayGoatTracker::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto* settings = metadata.Find<Settings>();
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);
    }

    void ReplayGoatTracker::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayGoatTracker::GetDurationMs() const
    {
        return 0;
    }

    uint32_t ReplayGoatTracker::GetNumSubsongs() const
    {
        return m_numSongs;
    }

    std::string ReplayGoatTracker::GetExtraInfo() const
    {
        std::string metadata;
        metadata = "Title    : ";
        metadata += songname;
        metadata += "\nArtist   : ";
        metadata += authorname;
        metadata += "\nCopyright: ";
        metadata += copyrightname;
        return metadata;
    }

    std::string ReplayGoatTracker::GetInfo() const
    {
        std::string info;
        info = "3 channels\n";
        info.append(reinterpret_cast<const char*>(&m_songId), 4);
        info += "\nGoatTracker " GoatTrackerVersion;
        return info;
    }
}
// namespace rePlayer