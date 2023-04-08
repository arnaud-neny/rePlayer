#pragma once

#include "Mutex.h"

namespace core::thread
{
    inline ScopedMutex::ScopedMutex(Mutex& mutex)
        : m_mutex(mutex)
    {
        mutex.Lock();
    }

    inline ScopedMutex::~ScopedMutex()
    {
        m_mutex.Unlock();
    }
}
// namespace core::thread