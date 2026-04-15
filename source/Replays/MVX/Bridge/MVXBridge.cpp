#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "resource.h"
#include "BridgeShared.h"

#include <Core.h>
#include <Core/String.h>

#include <csignal>

#define MVX_SAMPLERATE 44100

//////////////////////////////////////////////////////////////////////////

extern int mvxSystem_NumMachines;
extern BYTE* mvxSystem_MachineIDs;
extern BYTE* mvxSystem_MachineOrder;
extern int mvxSystem_NumConnections;

extern BYTE* mvxSystem_MasterSeq;
extern BYTE* mvxSystem_TPBSeq;
extern BYTE* mvxSystem_BPMSeq;
extern int mvxSystem_BPM;
extern int mvxSystem_TPB;
extern BYTE mvxSystem_MasterVolume;
extern int mvxSystem_MasterTick;
extern int mvxSystem_NumMasterTicks;

extern int* mvxSystem_ClipStart;
extern int* mvxSystem_ClipEnd;

extern int mvxSystem_SongLength; // in ticks
extern int mvxSystem_SamplesPerTick;

//////////////////////////////////////////////////////////////////////////

#define MVXCALL __fastcall

// MIXING ROUTINES
void MVXCALL mvxMixer_Init();
void MVXCALL mvxMixer_Render(float*, int);
void MVXCALL mvxMixer_DeInit();

// SYSTEM
int MVXCALL mvxSystem_LoadMusic(char* musicdata, int musicsize);
void MVXCALL mvxSystem_FreeMusic();

class mvxMachine
{
public:
    mvxMachine() {};
    virtual void Tick() = NULL;
    virtual int Render(float* buffer, int numsamp, float preamp) = NULL;
};

extern mvxMachine** mvxSystem_Machines;

HANDLE g_hResponseEvent = nullptr;
SharedMemory* g_sharedMemory = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int /*nCmdShow*/)
{
#if _DEBUG
    // so many leaks in MVX...
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

    g_hResponseEvent = hResponseEvent;
    g_sharedMemory = sharedMemory;
    struct
    {
        static LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* e)
        {
            g_sharedMemory->hasFailed = true;
            g_sharedMemory->bank[0].hasLooped = true;
            g_sharedMemory->bank[1].hasLooped = true;
            SetEvent(g_hResponseEvent);

            return EXCEPTION_CONTINUE_SEARCH;
        }
        static void OnAbort(int)
        {
            EXCEPTION_RECORD r = {
                .ExceptionCode = 0xBAADC0DE
            };
            EXCEPTION_POINTERS e = {
                .ExceptionRecord = &r
            };
            TopLevelExceptionFilter(&e);
            _exit(1); // avoid recursion
        }
    } cb;
    ::SetUnhandledExceptionFilter(&cb.TopLevelExceptionFilter);
    std::signal(SIGABRT, &cb.OnAbort);

    unsigned short fl = 0;
    __asm {
        fstcw[fl]
        or [fl], 0x0200
        fldcw[fl]
    }

    int err = 0;
    // Load Song
    if (mvxSystem_LoadMusic((char*)songData, (int)sharedMemory->songSize))
    {
        mvxMixer_Init();

        mvxSystem_MasterTick = 0;
        int64_t totalSamples = 0;
        for (int i = 0; i < mvxSystem_SongLength; ++i)
        {
            if (mvxSystem_MasterTick < mvxSystem_NumMasterTicks)
            {
                if (mvxSystem_TPBSeq[mvxSystem_MasterTick] != 0xFF)
                    mvxSystem_TPB = mvxSystem_TPBSeq[mvxSystem_MasterTick];
                if (mvxSystem_BPMSeq[mvxSystem_MasterTick] != 0xFF)
                    mvxSystem_BPM = mvxSystem_BPMSeq[mvxSystem_MasterTick];
            }
            totalSamples += (60 * MVX_SAMPLERATE) / (mvxSystem_TPB * mvxSystem_BPM);
            mvxSystem_MasterTick++;
        }
        mvxSystem_MasterTick = 0;
        sharedMemory->songDuration = uint32_t((totalSamples * 1000ll) / MVX_SAMPLERATE);
        sharedMemory->numMachines = uint32_t(mvxSystem_NumMachines);

        // unblock so client can continue
        SetEvent(hResponseEvent);

        int numSamples = 0;
        int currentTick = 0;
        for (;;)
        {
            if (sharedMemory->isQuitRequested)
                break;

            if (sharedMemory->isRestartRequested)
            {
                sharedMemory->isRestartRequested = false;
                sharedMemory->bank[0].hasLooped = false;
                sharedMemory->bank[1].hasLooped = false;

                // hardcore reset :(
                mvxMixer_DeInit();
                mvxSystem_FreeMusic();
                mvxSystem_LoadMusic((char*)songData, (int)sharedMemory->songSize);
                mvxMixer_Init();
                mvxSystem_MasterTick = 0;
                currentTick = 0;
                numSamples = 0;
            }

            int toFill = 1024;
            do
            {
                if (numSamples == 0)
                {
                    for (int i = 0; i < mvxSystem_NumMachines; ++i)
                    {
                        if (mvxSystem_Machines[i])
                            mvxSystem_Machines[i]->Tick();
                    }
                    if (mvxSystem_MasterTick < mvxSystem_NumMasterTicks)
                    {
                        if (mvxSystem_MasterSeq[mvxSystem_MasterTick] != 0xFF)
                            mvxSystem_MasterVolume = mvxSystem_MasterSeq[mvxSystem_MasterTick];
                        if (mvxSystem_TPBSeq[mvxSystem_MasterTick] != 0xFF)
                            mvxSystem_TPB = mvxSystem_TPBSeq[mvxSystem_MasterTick];
                        if (mvxSystem_BPMSeq[mvxSystem_MasterTick] != 0xFF)
                            mvxSystem_BPM = mvxSystem_BPMSeq[mvxSystem_MasterTick];
                        mvxSystem_SamplesPerTick = (60 * MVX_SAMPLERATE) / (mvxSystem_TPB * mvxSystem_BPM);
                    }
                    mvxSystem_MasterTick++;
                    numSamples = mvxSystem_SamplesPerTick;

                    currentTick++;
                    if (currentTick == mvxSystem_SongLength)
                    {
                        sharedMemory->bank[sharedMemory->bankIndex].hasLooped = true;
                        currentTick = 0;
                        mvxSystem_MasterTick = 0;
                    }
                }

                int toRender = numSamples < toFill ? numSamples : toFill;
                mvxMixer_Render(sharedMemory->bank[sharedMemory->bankIndex].samples + (1024 - toFill) * 2, toRender);

                numSamples -= toRender;
                toFill -= toRender;
            } while (toFill != 0);

            SetEvent(hResponseEvent);
            WaitForSingleObject(hRequestEvent, INFINITE);
        }

        mvxMixer_DeInit();
        mvxSystem_FreeMusic();
    }
    else
    {
        // unblock so the client can fail
        sharedMemory->hasFailed = true;
        SetEvent(hResponseEvent);
        err = -1;
    }
    delete[] songData;

    return err;
}
