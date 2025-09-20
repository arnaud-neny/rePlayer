#pragma once

#include "ReplayPlugin.h"

#include <windows.h>

#ifdef Yield
#   undef Yield
#endif

namespace rePlayer
{
    extern ReplayPlugin g_replayPlugin;
}
// namespace rePlayer

extern "C" __declspec(dllexport) rePlayer::ReplayPlugin* getReplayPlugin()
{
    return &rePlayer::g_replayPlugin;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
