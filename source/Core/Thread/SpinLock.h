#pragma once

#include <Core/Types.h>
#include <atomic>

namespace core::thread
{
    class SpinLock
    {
    public:
        void Lock();
        void Unlock();

    protected:
        alignas(64) std::atomic<uint32_t> m_state = 0;
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