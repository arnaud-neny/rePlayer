#pragma once

#include "SpinLock.h"

namespace core::thread
{
    inline void SpinLock::Lock()
    {
        while (m_state == 1 || m_state.exchange(1) == 1)
            _mm_pause();
    }

    inline void SpinLock::Unlock()
    {
        m_state.exchange(0);
    }

    inline ScopedSpinLock::ScopedSpinLock(SpinLock& spinLock)
        : m_spinLock(spinLock)
    {
        m_spinLock.Lock();
    }

    inline ScopedSpinLock::~ScopedSpinLock()
    {
        m_spinLock.Unlock();
    }
}
// namespace core::thread