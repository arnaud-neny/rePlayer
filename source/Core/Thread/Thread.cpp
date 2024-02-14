#include "Thread.h"

// Windows
#include <windows.h>
#include <intrin.h>

#ifdef Yield
#   undef Yield
#endif

namespace core::thread
{
    static thread_local ID s_id = ID::kUnknown;

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

    ID GetCurrentId()
    {
        return s_id;
    }

    void SetCurrentId(ID id)
    {
        s_id = id;
    }
}
// namespace core::thread