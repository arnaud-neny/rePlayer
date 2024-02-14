#pragma once

#include <Core.h>

namespace core::thread
{
    void Sleep(uint32_t milliseconds);
    void Yield();

    void SpinPause();
    void SpinMonitor(const void* ptr);
    void SpinWait();

    enum class ID
    {
        kUnknown,
        kMain,
        kWorker
    };
    ID GetCurrentId();
    void SetCurrentId(ID id);
}
// namespace core::thread