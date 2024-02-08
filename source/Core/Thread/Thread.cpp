#include "Thread.h"

// Windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>

#ifdef Yield
#undef Yield
#endif

namespace core::thread
{
    void Sleep(uint32_t milliseconds)
    {
        ::Sleep(milliseconds);
    }

    void Yield()
    {
        ::SwitchToThread();
    }

    void SpinPause()
    {
        _mm_pause();
    }

    void SpinMonitor(const void* ptr)
    {
        _mm_monitor(ptr, 0, 0);
    }

    void SpinWait()
    {
        _mm_mwait(0, 0);
    }
}
// namespace core::thread