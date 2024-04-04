#pragma once

#include "SpinLock.h"

namespace core::thread
{
    inline void SpinLock::Lock()
    {
        State newState = { 1 };
        newState.threadId = std::this_thread::get_id();
        static_assert(sizeof(std::thread::id) == sizeof(uint32_t));
        for(;;)
        {
            State oldState = { 0 };
            if (m_state.compare_exchange_strong(oldState.value, newState.value))
                break;
            if (oldState.threadId == newState.threadId)
            {
                m_state++;
                break;
            }
            _mm_pause();
        }
    }

    inline void SpinLock::Unlock()
    {
        State state = { --m_state };
        if (state.count == 0)
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