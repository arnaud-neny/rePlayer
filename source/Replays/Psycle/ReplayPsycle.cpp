#include "ReplayPsycle.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/Log.h>
#include <RePlayer/Version.h>
#include <ReplayDll.h>

#pragma comment(lib, "winmm.lib")

#include <psycle/host/Configuration.hpp>
#include <psycle/host/machineloader.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/host/ProgressDialog.hpp>
#include <psycle/plugins/druttis/blwtbl/blwtbl.h>
#include <filesystem>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Psycle, .isThreadSafe = false,
        .name = PSYCLE__TITLE,
        .extensions = "psy",
        .about = PSYCLE__TITLE PSYCLE__VERSION "\n" PSYCLE__COPYRIGHT,
        .load = ReplayPsycle::Load,
        .editMetadata = ReplayPsycle::Settings::Edit
    };

    struct StreamRiffFile : public psycle::host::RiffFile
    {
        bool Open(std::string const& FileName) override
        {
            return true;
        }

        bool Create(std::string const& FileName, bool overwrite) override
        {
            assert(0);
            return false;
        }

        bool Close() override
        {
            assert(0);
            return true;
        }
        bool Expect(const void* pData, std::size_t numBytes) override
        {
            return m_stream->Read((void*)pData, numBytes) == numBytes;
        }

        int Seek(std::size_t offset) override
        {
            return m_stream->Seek(offset, io::Stream::kSeekBegin) == Status::kOk;
        }

        int Skip(std::size_t numBytes) override
        {
            return m_stream->Seek(numBytes, io::Stream::kSeekCurrent) == Status::kOk;
        }

        bool Eof() override
        {
            return m_stream->GetPosition() >= m_stream->GetSize();
        }

        std::size_t FileSize() override
        {
            return m_stream->GetSize();
        }

        std::size_t GetPos() override
        {
            return m_stream->GetPosition();
        }

        FILE* GetFile() override
        {
            return nullptr;
        }

        bool ReadInternal(void* pData, std::size_t numBytes) override
        {
            return m_stream->Read(pData, numBytes) == numBytes;
        }

        bool WriteInternal(void const* pData, std::size_t numBytes) override
        {
            assert(0);
            return false;
        }

        SmartPtr<io::Stream> m_stream;
    };

    Replay* ReplayPsycle::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();
        if (data.Size() < 8)
            return nullptr;

        bool isPsyCle3 = memcmp(data.Items(), "PSY3SONG", 8) == 0;
        bool isPsyCle2 = !isPsyCle3 && memcmp(data.Items(), "PSY2SONG", 8) == 0;
        if (isPsyCle2 || isPsyCle3)
        {
            psycle::plugins::druttis::InitLibrary();
            psycle::plugins::druttis::UpdateWaveforms(kSampleRate);

            psycle::host::Global::pLogCallback = [](const char* msg, const char* title)
            {
                Log::Error("Psycle: %s / %s\n", title, msg);
            };

            auto* settings = metadata.Find<Settings>();
            psycle::host::Global::configuration().LoadNewBlitz(settings && settings->loadNewBlitz);

            psycle::host::Global::configuration().SetNumThreads(Max(2, std::thread::hardware_concurrency() - 2));
            psycle::host::Global::configuration().RefreshSettings();

            psycle::host::Global::configuration().SetPluginDir("");
            auto mainPath = std::filesystem::current_path() / "replays" REPLAYER_OS_STUB / "Psycle" / "VstPlugins64";
            psycle::host::Global::configuration().SetVst64Dir(LPCSTR(mainPath.u8string().c_str()));
            mainPath = std::filesystem::current_path() / "replays" REPLAYER_OS_STUB / "Psycle" / "VstPlugins";
            psycle::host::Global::configuration().SetVst32Dir(LPCSTR(mainPath.u8string().c_str()));
            mainPath = std::filesystem::current_path() / "replays" REPLAYER_OS_STUB / "Psycle" / "LadspaPlugins";
            psycle::host::Global::configuration().SetLadspaDir(LPCSTR(mainPath.u8string().c_str()));
            mainPath = std::filesystem::current_path() / "replays" REPLAYER_OS_STUB / "Psycle" / "LuaScripts";
            psycle::host::Global::configuration().SetLuaDir(LPCSTR(mainPath.u8string().c_str()));

            psycle::host::Global::machineload().ReScan();

            psycle::host::CProgressDialog dlg;
            StreamRiffFile riffFile;
            riffFile.m_stream = stream;
            if (psycle::host::Global::song().Load(&riffFile, dlg))
                return new ReplayPsycle(isPsyCle3);

            psycle::plugins::druttis::CloseLibrary();
        }

        return nullptr;
    }

    void ReplayPsycle::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* const blitz[] = { "Load Old Blitz", "Load New Blitz" };
        auto loadNewBlitz = int(entry->loadNewBlitz);
        ImGui::Combo("##Combo", &loadNewBlitz, blitz, NumItemsOf(blitz));
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Only at loading if the song is using\n\"blitz.dll\" or \"gamefx.dll\"");
        entry->loadNewBlitz = uint32_t(loadNewBlitz);

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplayPsycle::~ReplayPsycle()
    {
        psycle::host::Global::player().Stop();
        psycle::host::Global::song().Reset();

        // force the player delete to avoid some memory leaks with dll hack
        psycle::host::Global::player().~Player();
        new (&psycle::host::Global::player()) psycle::host::Player;

        psycle::plugins::druttis::CloseLibrary();
    }

    ReplayPsycle::ReplayPsycle(bool isPsycle3)
        : Replay(eExtension::_psy, eReplay::Psycle)
        , m_isPsycle3(isPsycle3)
    {
        psycle::host::Global::player()._recording = true;
        m_clipboardMem.push_back(reinterpret_cast<char*>(&m_clipboarMemSize));
        m_clipboardMem.push_back(nullptr);
        psycle::host::Global::player().StartRecording("", 32, kSampleRate, psycle::host::no_mode, true, false, 0, 0, &m_clipboardMem);
        psycle::host::Global::player().Start(0, 0);
    }

    uint32_t ReplayPsycle::Render(StereoSample* output, uint32_t numSamples)
    {
        m_clipboarMemSize = 0;
        m_clipboardMem[1] = reinterpret_cast<char*>(output);
        psycle::host::Global::player().Work(numSamples);
        return uint32_t(psycle::host::Global::player().sampleOffset);
    }

    void ReplayPsycle::ResetPlayback()
    {
        psycle::host::Global::player().Start(0, 0);
    }

    void ReplayPsycle::ApplySettings(const CommandBuffer metadata)
    {
        UnusedArg(metadata);
    }

    void ReplayPsycle::SetSubsong(uint32_t subsongIndex)
    {
        UnusedArg(subsongIndex);
        ResetPlayback();
    }

    uint32_t ReplayPsycle::GetDurationMs() const
    {
        int seq = -1;
        int pos = -1;
        int time = -1;
        int linecountloc = -1;
        psycle::host::Global::player().CalcPosition(psycle::host::Global::song(), seq, pos, time, linecountloc);
        return uint32_t(time);
    }

    uint32_t ReplayPsycle::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayPsycle::GetExtraInfo() const
    {
        std::string metadata = "Name    : ";
        metadata += psycle::host::Global::song().name;
        metadata += "\nAuthor  : ";
        metadata += psycle::host::Global::song().author;
        metadata += "\nComments: ";
        metadata += psycle::host::Global::song().comments;
        return metadata;
    }

    std::string ReplayPsycle::GetInfo() const
    {
        std::string info;
        char buf[16];
        sprintf(buf, "%d", psycle::host::Global::song().SONGTRACKS);
        info = buf;
        info += " channels\n";
        info += m_isPsycle3 ? "New" : "Old";
        info += " Format\nPsycle" PSYCLE__VERSION;
        return info;
    }
}
// namespace rePlayer