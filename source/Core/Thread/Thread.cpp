#include "Thread.h"

// stl
#include <atomic>

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

    int32_t KeepAwake(bool isEnabled)
    {
        static std::atomic<int32_t> awakeCounter = 0;
        if (isEnabled)
        {
            if (!awakeCounter++)
                SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
        }
        else
        {
            if (!--awakeCounter)
                SetThreadExecutionState(ES_CONTINUOUS);
        }
        return awakeCounter;
    }
}
// namespace core::thread