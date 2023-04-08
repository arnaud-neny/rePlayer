#pragma once

#include <Core/Types.h>
#include <atomic>

namespace core::thread
{
    class alignas(64) SpinLock
    {
    public:
        void Lock();
        void Unlock();

    protected:
        std::atomic<uint32_t> m_state = 0;
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