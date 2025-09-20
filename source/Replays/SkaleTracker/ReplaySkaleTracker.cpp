#include "ReplaySkaleTracker.h"

#include <Core/String.h>
#include <Thread/Thread.h>
#include <ReplayDll.h>
#include <RePlayer/Version.h>

#ifdef _DEBUG
#include <filesystem>
#endif

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SkaleTracker,
        .name = "Skale Tracker",
        .extensions = "skm",
        .about = "Skale Tracker 0.81\nCopyright (c) 2005 Ruben Ramos Salvador aka Baktery/Chanka",
        .load = ReplaySkaleTracker::Load
    };

    Replay* ReplaySkaleTracker::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        // check header
        const uint8_t magic[] = { 1, 0, 0xfe, 0xff, 9, 0, 0, 0, 0x41, 0x4c, 0x49, 0x4d, 0x33 };
        uint8_t header[sizeof(magic)];
        stream->Read(header, sizeof(header));
        if (memcmp(header, magic, sizeof(magic)))
            return nullptr;
        stream->Seek(0, io::Stream::kSeekBegin);

        // prepare bridge
        static uint32_t s_id = 0;
        char id[9];
        sprintf(id, "%08X", s_id++);

        auto hFileMapping = core::Scope([&]()
        {
            char txt[1024];
            core::sprintf(txt, SHARED_MEMORY_FORMAT, id);
            return CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedMemory), txt);
        }, [](auto data) { if (data) CloseHandle(data); });
        if (!hFileMapping)
            return nullptr;

        auto sharedMemory = core::Scope([&]() { return (SharedMemory*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemory)); },
            [](auto data) { if (data) UnmapViewOfFile(data); });
        if (!sharedMemory)
            return nullptr;

        auto hRequestEvent = core::Scope([&]()
        {
            char txt[1024];
            core::sprintf(txt, REQUEST_EVENT_FORMAT, id);
            return CreateEventA(nullptr, FALSE, FALSE, txt);
        }, [](auto data) { if (data) CloseHandle(data); });
        if (!hRequestEvent)
            return nullptr;

        auto hResponseEvent = core::Scope([&]()
        {
            char txt[1024];
            core::sprintf(txt, RESPONSE_EVENT_FORMAT, id);
            return CreateEventA(nullptr, FALSE, FALSE, txt);
        }, [](auto data) { if (data) CloseHandle(data); });
        if (!hResponseEvent)
            return nullptr;

        // initialize memory bridge
        sharedMemory->isQuitRequested = false;
        sharedMemory->isRestartRequested = false;
        sharedMemory->hasFailed = false;
        sharedMemory->bankIndex = 0;
        sharedMemory->songSize = uint32_t(stream->GetSize());

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

#ifdef _DEBUG
        char* pgrPath;
        _get_pgmptr(&pgrPath);
        std::string bridgePath;
        {
            auto path = std::filesystem::path(pgrPath).remove_filename() / "replays" REPLAYER_OS_STUB / "SkaleTracker\\SkaleTrackerBridge.exe";
            path = path.lexically_normal();
            bridgePath = path.generic_string();
        }
        auto* bridgePathStr = bridgePath.c_str();
#else
        const char* bridgePathStr = "replays" REPLAYER_OS_STUB "\\SkaleTracker\\SkaleTrackerBridge.exe";
#endif

        if (CreateProcessA(bridgePathStr, id, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi))
        {
            for (uint32_t offset = 0; offset < sharedMemory->songSize;)
            {
                uint32_t toRead = Min(uint32_t(sizeof(sharedMemory->songData)), sharedMemory->songSize - offset);
                stream->Read(sharedMemory->songData, toRead);
                SetEvent(hRequestEvent);
                offset += toRead;
                while (WaitForSingleObject(hResponseEvent, 0) != WAIT_OBJECT_0)
                {
                    // check if the bridge has failed
                    if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0)
                    {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);

                        return nullptr;
                    }
                    core::thread::Yield();
                }
            }
            if (sharedMemory->hasFailed)
                return nullptr;
            return new ReplaySkaleTracker(pi.hProcess, pi.hThread, hFileMapping.Detach(), hRequestEvent.Detach(), hResponseEvent.Detach(), sharedMemory.Detach());
        }

        return nullptr;
    }

    ReplaySkaleTracker::~ReplaySkaleTracker()
    {
        m_sharedMemory->isQuitRequested = true;
        SetEvent(m_hRequestEvent);

        WaitForSingleObject(m_hProcess, INFINITE);

        CloseHandle(m_hProcess);
        CloseHandle(m_hThread);

        UnmapViewOfFile(m_sharedMemory);
        CloseHandle(m_hFileMapping);
        CloseHandle(m_hRequestEvent);
        CloseHandle(m_hResponseEvent);
    }

    ReplaySkaleTracker::ReplaySkaleTracker(void* hProcess, void* hThread, void* hFileMapping, void* hRequestEvent, void* hResponseEvent, SharedMemory* sharedMemory)
        : Replay(eExtension::_skm, eReplay::SkaleTracker)
        , m_hProcess(hProcess)
        , m_hThread(hThread)
        , m_hFileMapping(hFileMapping)
        , m_hRequestEvent(hRequestEvent)
        , m_hResponseEvent(hResponseEvent)
        , m_sharedMemory(sharedMemory)
    {}

    uint32_t ReplaySkaleTracker::Render(StereoSample* output, uint32_t numSamples)
    {
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (mNumSamples == 0)
            {
                if (m_sharedMemory->bank[m_sharedMemory->bankIndex ^ 1].hasLooped)
                {
                    if (remainingSamples == numSamples)
                        m_sharedMemory->bank[m_sharedMemory->bankIndex ^ 1].hasLooped = false;
                    break;
                }

                WaitForSingleObject(m_hResponseEvent, INFINITE);

                mNumSamples = 1024;
                m_sharedMemory->bankIndex ^= 1;
                SetEvent(m_hRequestEvent);
            }
            auto toCopy = Min(mNumSamples, remainingSamples);
            auto index = 1024 - mNumSamples;
            memcpy(output, m_sharedMemory->bank[m_sharedMemory->bankIndex ^ 1].samples + index * 2, sizeof(float) * toCopy * 2);
            mNumSamples -= toCopy;
            remainingSamples -= toCopy;
            output += toCopy;
        }

        return numSamples - remainingSamples;
    }

    void ReplaySkaleTracker::ResetPlayback()
    {
        WaitForSingleObject(m_hResponseEvent, INFINITE);

        m_sharedMemory->isRestartRequested = true;
        m_sharedMemory->bankIndex = 0;
        mNumSamples = 0;

        SetEvent(m_hRequestEvent);
    }

    void ReplaySkaleTracker::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplaySkaleTracker::SetSubsong(uint32_t)
    {}

    uint32_t ReplaySkaleTracker::GetDurationMs() const
    {
        return 0;
    }

    uint32_t ReplaySkaleTracker::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplaySkaleTracker::GetExtraInfo() const
    {
        return m_sharedMemory->title;
    }

    std::string ReplaySkaleTracker::GetInfo() const
    {
        std::string info;
        info = "2 channels\nSkale Player\nSkale Tracker 0.81";
        return info;
    }
}
// namespace rePlayer