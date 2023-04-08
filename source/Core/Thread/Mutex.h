#pragma once

#include <Core/Types.h>

namespace core::thread
{
    class Mutex
    {
    public:
        Mutex(uint32_t numSpinLocks = 0);
        ~Mutex();

        void Lock();
        bool TryLock();
        void Unlock();

    private:
        Mutex(const Mutex&) = delete;
        Mutex(Mutex&&) = delete;
        Mutex& operator=(const Mutex&) = delete;
        Mutex& operator=(Mutex&&) = delete;

    private:
        uint64_t m_buffer[8];
    };

    class ScopedMutex
    {
    public:
        explicit ScopedMutex(Mutex& mutex);
        ~ScopedMutex();

    private:
        Mutex& m_mutex;
    };
}
// namespace core::thread

#include "Mutex.inl.h"