#include "RefCounted.h"
#include <atomic>

namespace core
{
    RefCounted::~RefCounted()
    {}

    void RefCounted::AddRef()
    {
        std::atomic_ref atomicRef(m_refCount);
        atomicRef++;
    }

    void RefCounted::Release()
    {
        std::atomic_ref atomicRef(m_refCount);
        if (--atomicRef == 0)
            delete this;
    }
}
// namespace core