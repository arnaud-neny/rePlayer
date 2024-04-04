#pragma once

#include <Core/Types.h>
#include <atomic>
#include <thread>

namespace core::thread
{
    class SpinLock
    {
    public:
        void Lock();
        void Unlock();

    private:
        union State
        {
            uint64_t value = 0;
            struct
            {
                uint32_t count;
                std::thread::id threadId;
            };
        };

    private:
        alignas(64) std::atomic<uint64_t> m_state = 0;
    };

    class ScopedSpinLock
    {
    public:
        explicit ScopedSpinLock(SpinLock& spinLock);
        ~ScopedSpinLock();

    private:
        SpinLock& m_spinLock;
    };
}
// namespace core::thread

#include "SpinLock.inl.h"