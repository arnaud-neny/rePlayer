#include "Mutex.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace core::thread
{
    Mutex::Mutex(uint32_t numSpinLocks)
    {
        static_assert(sizeof(CRITICAL_SECTION) <= sizeof(m_buffer));
#ifdef _DEBUG
        constexpr uint32_t flags = 0;
#else
        constexpr uint32_t flags = CRITICAL_SECTION_NO_DEBUG_INFO;
#endif
        ::InitializeCriticalSectionEx(reinterpret_cast<CRITICAL_SECTION*>(m_buffer), numSpinLocks, flags);
    }

    Mutex::~Mutex()
    {
        ::DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_buffer));
    }

    void Mutex::Lock()
    {
        ::EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_buffer));
    }

    void Mutex::Unlock()
    {
        ::LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_buffer));
    }
}
// namespace core::thread