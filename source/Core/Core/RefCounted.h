#pragma once

#include "Types.h"

namespace core
{
    class RefCounted
    {
    public:
        RefCounted() = default;
        virtual ~RefCounted();

        virtual void AddRef() final;
        virtual void Release() final;
        virtual void OnDelete() {};

    protected:
        uint32_t m_refCount = 0;
    };
}
// namespace core