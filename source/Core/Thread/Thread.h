#pragma once

#include <Core.h>

namespace core::thread
{
    void Sleep(uint32_t milliseconds);
    void Yield();

    void SpinPause();
    void SpinMonitor(const void* ptr);
    void SpinWait();
}
// namespace core::thread