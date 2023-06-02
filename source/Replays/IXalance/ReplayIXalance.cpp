#include "ReplayIXalance.h"

#include "webixs/Module.h"
#include "webixs/PlayerCore.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <IO/File.h>
#include <ReplayDll.h>

#include <dllloader.h>

#include <bit>
#include <filesystem>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::iXalance,
        .name = "iXalance",
        .extensions = "ixs",
        .about = "iXalance\nCopyright (c) 2022 Juergen Wothke\nCopyright (c) original x86 code : Shortcut Software Development BV",
        .init = ReplayIXalance::Init,
        .release = ReplayIXalance::Release,
        .load = ReplayIXalance::Load
    };

    // Dll hook begin
    static DllManager* s_dllManager = nullptr;
    static bool s_isMainModule = false;
    struct DllEntry
    {
        std::wstring path;
        HMODULE handle;
        ReplayIXalance* replay;
    };
    static Array<DllEntry> s_dlls;
    SharedContexts* s_sharedContexts = nullptr;

    typedef ReplayPlugin* (*GetReplayPlugin)();
    // Dll hook end

    // todo remove?
    bool ReplayIXalance::Init(SharedContexts* ctx, Window& window)
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

    void ReplayIXalance::Release()
    {
        for (auto& dllEntry : s_dlls)
        {
            ::FreeLibrary(dllEntry.handle);
            s_dllManager->UnsetDllFile(dllEntry.path.c_str());
        }
        s_dlls = {};
        delete s_dllManager;
    }

    Replay* ReplayIXalance::Load(io::Stream* stream, CommandBuffer metadata)
    {
        uint32_t magic = 0;
        stream->Read(&magic, sizeof(magic));
        if (magic != 0x21535849)
            return nullptr;
        stream->Seek(0, io::Stream::SeekWhence::kSeekBegin);

        if (s_isMainModule)
        {
            // Once again, some static data are here, so keep it thread safe.
            // Thanx to dll manager for that.
            // We still have to share some data from the main dll (iXalance.dll) with the others such as the SharedContext...

            // Load the main dll in memory
            char* pgrPath;
            _get_pgmptr(&pgrPath);
            auto mainPath = std::filesystem::path(pgrPath).remove_filename() / "replays/iXalance.dll";
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
            sprintf(dllName, "iXalance%08X.dll", counter++);

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
            ReplayIXalance* replay = nullptr;
            if (dllHandle != 0)
            {
                // load the song though the new module
                auto g = reinterpret_cast<GetReplayPlugin>(GetProcAddress(dllHandle, "getReplayPlugin"));
                Window* w = nullptr;
                g()->init(s_sharedContexts, reinterpret_cast<Window&>(*w));
                replay = reinterpret_cast<ReplayIXalance*>(g()->load(stream, metadata));
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

        IXS::PlayerIXS* player = IXS::IXS__PlayerIXS__createPlayer_00405d90(kSampleRate);
        float progress = 0.0f;
        auto data = stream->Read();
        int r = (player->vftable->loadIxsFileData)(player, (byte*)data.Items(), uint32_t(data.Size()), 0, 0, &progress);
        if (r == 0)
        {
            (player->vftable->initAudioOut)(player);  // depends on loaded song
            return new ReplayIXalance(player);
        }

        player->vftable->delete0(player);
        return nullptr;
    }

    ReplayIXalance::~ReplayIXalance()
    {
        m_player->vftable->delete0(m_player);
        if (m_dllEntries)
            (*m_dllEntries)[m_dllIndex].replay = nullptr;
    }

    ReplayIXalance::ReplayIXalance(IXS::PlayerIXS* player)
        : Replay(eExtension::_ixs, eReplay::iXalance)
        , m_player(player)
    {}

    uint32_t ReplayIXalance::Render(StereoSample* output, uint32_t numSamples)
    {
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_size != 0)
            {
                auto numToConvert = Min(m_size, remainingSamples);
                output = output->Convert(m_buffer, numToConvert);
                m_buffer += numToConvert * 2;
                m_size -= numToConvert;
                remainingSamples -= numToConvert;
            }
            else
            {
                auto loopCount = m_player->vftable->isSongEnd(m_player);
                if (loopCount != m_lastLoop)
                {
                    m_lastLoop = loopCount;
                    return 0;
                }
                (m_player->vftable->genAudio)(m_player);

                m_buffer = reinterpret_cast<int16_t*>((m_player->vftable->getAudioBuffer)(m_player));

                auto is16bit = (m_player->vftable->isAudioOutput16Bit)(m_player);
                auto isStereo = (m_player->vftable->isAudioOutputStereo)(m_player);
                assert(is16bit && isStereo);

                m_size = (m_player->vftable->getAudioBufferLen)(m_player);
            }
        }

        return numSamples;
    }

    void ReplayIXalance::ResetPlayback()
    {
        m_lastLoop = 0;
        (m_player->vftable->initAudioOut)(m_player);
    }

    void ReplayIXalance::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayIXalance::SetSubsong(uint16_t)
    {}

    uint32_t ReplayIXalance::GetDurationMs() const
    {
        return 0;
    }

    uint32_t ReplayIXalance::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayIXalance::GetExtraInfo() const
    {
        std::string info;
        info = (m_player->vftable->getSongTitle)(m_player);
        bool doReturn = false;
        for (uint32_t i = 0; i < m_player->ptrCore_0x4->ptrModule_0x8->impulseHeader_0x0.SmpNum_0x24; i++)
        {
            if (m_player->ptrCore_0x4->ptrModule_0x8->smplHeadPtrArr0_0xd0[i]->filename_0x4[0] != 0)
            {
                if (doReturn = !doReturn)
                    info += "\n";
                else
                    info += " / ";
                info += m_player->ptrCore_0x4->ptrModule_0x8->smplHeadPtrArr0_0xd0[i]->filename_0x4;
            }
        }
        return info;
    }

    std::string ReplayIXalance::GetInfo() const
    {
        char buf[64];
        sprintf(buf, "%d channels\niXalance\nWebIXS/IXSPlayer 1.20", std::popcount(m_player->ptrCore_0x4->ptrModule_0x8->channels));
        return buf;
    }
}
// namespace rePlayer