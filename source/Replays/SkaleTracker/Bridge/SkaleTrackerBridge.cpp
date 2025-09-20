#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "resource.h"
#include "BridgeShared.h"
#include "../SkalePlayer/Include/SkalePlayer.h"

#include <Core.h>
#include <Core/String.h>

#pragma comment(lib, "../SkalePlayer/Lib/Win32/SkalePlayer.lib")

class SkalePlayerEvent : public ISkalePlayer::IEventHandler
{
public:
    void Event(EEvent eEvent, const TEventInfo* pEventInfo) override
    {
        if (eEvent == ISkalePlayer::IEventHandler::EVENT_ENDOFSONG)
        {
            m_hasLooped = true;
        }
    }

    bool m_hasLooped = false;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int /*nCmdShow*/)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(1);
#endif

    auto id = GetCommandLineA();
    auto hFileMapping = core::Scope([&]()
        {
            char txt[1024];
            core::sprintf(txt, SHARED_MEMORY_FORMAT, id);
            return OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, txt);
        }, [](auto data) { if (data) CloseHandle(data); });
    if (!hFileMapping)
        return -1;

    auto sharedMemory = core::Scope([&]() { return (SharedMemory*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemory)); },
        [](auto data) { if (data) UnmapViewOfFile(data); });
    if (!sharedMemory)
        return -1;

    auto hRequestEvent = core::Scope([&]()
        {
            char txt[1024];
            core::sprintf(txt, REQUEST_EVENT_FORMAT, id);
            return OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, txt);
        }, [](auto data) { if (data) CloseHandle(data); });
    if (!hRequestEvent)
        return -1;
        
    auto hResponseEvent = core::Scope([&]()
        {
            char txt[1024];
            core::sprintf(txt, RESPONSE_EVENT_FORMAT, id);
            return OpenEventA(EVENT_MODIFY_STATE, FALSE, txt);
        }, [](auto data) { if (data) CloseHandle(data); });
    if (!hResponseEvent)
        return -1;

    // Must init with Slave Mode
    ISkalePlayer::TInitData initData;
    initData.m_eMode = ISkalePlayer::INIT_MODE_SLAVE;

    // Init Skale Player
    ISkalePlayer* skalePlayer = ISkalePlayer::GetSkalePlayer();
    ISkalePlayer::EInitError error = skalePlayer->Init(initData);

    if (error == ISkalePlayer::INIT_OK)
    {
        // Setup our Event Handler
        SkalePlayerEvent event;
        skalePlayer->SetEventHandler(&event);

        // get the song from rePlayer
        auto* songData = new uint8_t[sharedMemory->songSize];
        for (uint32_t offset = 0;;)
        {
            WaitForSingleObject(hRequestEvent, INFINITE);
            auto toCopy = core::Min(sharedMemory->songSize - offset, 65536u);
            memcpy(songData + offset, sharedMemory->songData, toCopy);
            offset += toCopy;
            sharedMemory->bank[0].hasLooped = false;
            sharedMemory->bank[1].hasLooped = false;
            if (offset < sharedMemory->songSize)
                SetEvent(hResponseEvent);
            else
                break;
        }

        // Load Song
        const ISkalePlayer::ISong* song = skalePlayer->LoadSongFromMemory(songData, sharedMemory->songSize);
        if (song)
        {
            // unblock so client can continue
            SetEvent(hResponseEvent);

            // get the song title 
            ISkalePlayer::ISong::TInfo info;
            song->GetInfo(&info);
            memcpy(sharedMemory->title, info.m_szTitle, sizeof(sharedMemory->title));

            // Set loaded song as active
            skalePlayer->SetCurrentSong(song);

            // Play
            skalePlayer->SetPlayMode(ISkalePlayer::PLAY_MODE_PLAY_SONG_FROM_START);

            for (;;)
            {
                if (sharedMemory->isQuitRequested)
                    break;

                if (sharedMemory->isRestartRequested)
                {
                    sharedMemory->isRestartRequested = false;
                    sharedMemory->bank[0].hasLooped = false;
                    sharedMemory->bank[1].hasLooped = false;
                    event.m_hasLooped = false;
                    // had to reload song as the set play mode only doesn't work 
                    skalePlayer->FreeSong(song);
                    song = skalePlayer->LoadSongFromMemory(songData, sharedMemory->songSize);
                    skalePlayer->SetCurrentSong(song);
                    skalePlayer->SetPlayMode(ISkalePlayer::PLAY_MODE_PLAY_SONG_FROM_START);
                }

                skalePlayer->SlaveProcess(sharedMemory->bank[sharedMemory->bankIndex].samples, 1024);
                sharedMemory->bank[sharedMemory->bankIndex].hasLooped = event.m_hasLooped;
                event.m_hasLooped = false;

                SetEvent(hResponseEvent);
                WaitForSingleObject(hRequestEvent, INFINITE);
            }

            skalePlayer->FreeSong(song);
        }
        else
        {
            // unblock so the client can fail
            sharedMemory->hasFailed = true;
            SetEvent(hResponseEvent);
        }
        delete[] songData;

        // Uninit SkalePlayer
        skalePlayer->End();
    }
    else
        return -1;

    return 0;
}
