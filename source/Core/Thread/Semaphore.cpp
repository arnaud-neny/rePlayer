#include "Semaphore.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace core::thread
{
    Semaphore::Semaphore(uint32_t initialCount, uint32_t maxCount)
    {
        static_assert(sizeof(HANDLE) <= sizeof(m_handle));
        m_handle = ::CreateSemaphoreExW(nullptr, initialCount, maxCount, nullptr, 0, SEMAPHORE_ALL_ACCESS);
    }

    Semaphore::~Semaphore()
    {
        ::CloseHandle(m_handle);
    }

    void Semaphore::Signal(uint32_t numEvents)
    {
        ::ReleaseSemaphore(reinterpret_cast<HANDLE>(m_handle), LONG(numEvents), nullptr);
    }

    void Semaphore::Wait()
    {
        ::WaitForSingleObject(reinterpret_cast<HANDLE>(m_handle), INFINITE);
    }
}
// namespace core::thread