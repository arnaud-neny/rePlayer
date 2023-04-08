#pragma once

#include <Core/Types.h>

namespace core::thread
{
    class Semaphore
    {
    public:
        Semaphore(uint32_t initialCount = 0, uint32_t maxCount = 0x7fFFffFF);
        ~Semaphore();

        void Signal(uint32_t numEvents = 1);
        void Wait();

    private:
        Semaphore(const Semaphore&) = delete;
        Semaphore(Semaphore&&) = delete;
        Semaphore& operator=(const Semaphore&) = delete;
        Semaphore& operator=(Semaphore&&) = delete;

    private:
        void* m_handle;
    };
}
// namespace core::thread

#include "Mutex.inl.h"