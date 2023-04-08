#pragma once

#include "SmartPtr.h"

namespace core
{
    template <typename RefCountClass>
    template<typename... Args>
    inline SmartPtr<RefCountClass>::SmartPtr(AutoAllocate, Args&&... args)
    {
        m_ptr = new RefCountClass(std::forward<Args>(args)...);
        m_ptr->AddRef();
    }

    template <typename RefCountClass>
    inline SmartPtr<RefCountClass>::SmartPtr(RefCountClass* ptr)
        : m_ptr(ptr)
    {
        if (ptr)
        {
            ptr->AddRef();
        }
    }

    template <typename RefCountClass>
    inline SmartPtr<RefCountClass>::SmartPtr(const SmartPtr<RefCountClass>& smartPtr)
        : m_ptr(smartPtr.m_ptr)
    {
        if (m_ptr)
        {
            m_ptr->AddRef();
        }
    }

    template <typename RefCountClass>
    inline SmartPtr<RefCountClass>::SmartPtr(SmartPtr<RefCountClass>&& smartPtr)
        : m_ptr(smartPtr.m_ptr)
    {
        smartPtr.m_ptr = nullptr;
    }

    template <typename RefCountClass>
    template <typename OtherRefCountClass>
    inline SmartPtr<RefCountClass>::SmartPtr(SmartPtr<OtherRefCountClass>&& smartPtr)
        : m_ptr(static_cast<RefCountClass*>(smartPtr.m_ptr))
    {
        smartPtr.m_ptr = nullptr;
    }

    template <typename RefCountClass>
    inline SmartPtr<RefCountClass>::~SmartPtr()
    {
        if (m_ptr)
        {
            m_ptr->Release();
        }
    }

    template <typename RefCountClass>
    inline bool SmartPtr<RefCountClass>::IsValid() const
    {
        return m_ptr != nullptr;
    }

    template <typename RefCountClass>
    inline bool SmartPtr<RefCountClass>::IsInvalid() const
    {
        return m_ptr == nullptr;
    }

    template <typename RefCountClass>
    template <typename T>
    inline T* SmartPtr<RefCountClass>::As() const
    {
        return reinterpret_cast<T*>(m_ptr);
    }

    template <typename RefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::Get() const
    {
        return m_ptr;
    }

    template <typename RefCountClass>
    inline void SmartPtr<RefCountClass>::Attach(RefCountClass* ptr)
    {
        if (m_ptr)
            m_ptr->Release();
        m_ptr = ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::Detach()
    {
        auto ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    template <typename RefCountClass>
    template<typename... Args>
    inline SmartPtr<RefCountClass>& SmartPtr<RefCountClass>::New(Args&&... args)
    {
        if (m_ptr)
            m_ptr->Release();
        m_ptr = new RefCountClass(std::forward<Args>(args)...);
        m_ptr->AddRef();

        return *this;
    }

    template <typename RefCountClass>
    inline void SmartPtr<RefCountClass>::Reset()
    {
        if (m_ptr)
        {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

    template <typename RefCountClass>
    inline SmartPtr<RefCountClass>::operator RefCountClass*() const
    {
        return m_ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass& SmartPtr<RefCountClass>::operator*() const
    {
        return *m_ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass** SmartPtr<RefCountClass>::operator&()
    {
        return &m_ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::operator->() const
    {
        return m_ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::operator=(RefCountClass* ptr)
    {
        if (ptr)
            ptr->AddRef();
        if (m_ptr)
            m_ptr->Release();
        m_ptr = ptr;
        return ptr;
    }

    template <typename RefCountClass>
    template <typename OtherRefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::operator=(const SmartPtr<OtherRefCountClass>& smartPtr)
    {
        *this = smartPtr.m_ptr;
        return smartPtr.m_ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::operator=(const SmartPtr<RefCountClass>& smartPtr)
    {
        *this = smartPtr.m_ptr;
        return smartPtr.m_ptr;
    }

    template <typename RefCountClass>
    inline RefCountClass* SmartPtr<RefCountClass>::operator=(SmartPtr<RefCountClass>&& smartPtr)
    {
        if (m_ptr && m_ptr != smartPtr.m_ptr)
            m_ptr->Release();
        m_ptr = smartPtr.m_ptr;
        smartPtr.m_ptr = nullptr;
        return m_ptr;
    }

    template <typename RefCountClass>
    inline bool SmartPtr<RefCountClass>::operator!() const
    {
        return m_ptr == nullptr;
    }

    template <typename RefCountClass>
    inline bool SmartPtr<RefCountClass>::operator<(RefCountClass* ptr) const
    {
        return m_ptr < ptr;
    }

    template <typename RefCountClass>
    inline bool SmartPtr<RefCountClass>::operator==(RefCountClass* ptr) const
    {
        return m_ptr == ptr;
    }

    template <typename RefCountClass>
    inline bool SmartPtr<RefCountClass>::operator!=(RefCountClass* ptr) const
    {
        return m_ptr != ptr;
    }
}
// namespace core