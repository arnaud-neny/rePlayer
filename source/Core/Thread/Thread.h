#pragma once

#include <Core.h>

namespace core::thread
{
    void Sleep(uint32_t milliseconds);
    void Yield();

    void SpinPause();
    void SpinMonitor(const void* ptr);
    void SpinWait();

    int32_t KeepAwake(bool isEnabled); // return < 0 then will not be kept awake

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