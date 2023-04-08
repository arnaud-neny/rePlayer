#pragma once

#include <Core.h>

namespace core
{
    template <typename RefCountClass>
    class SmartPtr
    {
        template <typename OtherRefCountClass>
        friend class SmartPtr;

    public:
        SmartPtr() = default;
        template<typename... Args>
        SmartPtr(AutoAllocate, Args&&... args);
        SmartPtr(RefCountClass* ptr);
        SmartPtr(const SmartPtr<RefCountClass>& smartPtr);
        SmartPtr(SmartPtr<RefCountClass>&& smartPtr);
        template <typename OtherRefCountClass>
        SmartPtr(SmartPtr<OtherRefCountClass>&& smartPtr);
        ~SmartPtr();

        bool IsValid() const;
        bool IsInvalid() const;

        template <typename T>
        T* As() const;
        RefCountClass* Get() const;

        void Attach(RefCountClass* ptr);
        RefCountClass* Detach();
        template<typename... Args>
        SmartPtr& New(Args&&... args);
        void Reset();

        operator RefCountClass*() const;
        RefCountClass& operator*() const;
        RefCountClass** operator&();

        RefCountClass* operator->() const;

        RefCountClass* operator=(RefCountClass* ptr);
        template <typename OtherRefCountClass>
        RefCountClass* operator=(const SmartPtr<OtherRefCountClass>& smartPtr);
        RefCountClass* operator=(const SmartPtr<RefCountClass>& smartPtr);
        RefCountClass* operator=(SmartPtr<RefCountClass>&& smartPtr);

        bool operator!() const;
        bool operator<(RefCountClass* ptr) const;
        bool operator==(RefCountClass* ptr) const;
        bool operator!=(RefCountClass* ptr) const;

    protected:
        RefCountClass* m_ptr{ nullptr };
    };
}
// namespace core

#include "SmartPtr.inl.h"