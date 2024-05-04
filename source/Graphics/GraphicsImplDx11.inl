#pragma once

#ifndef _WIN64
#include "GraphicsImplDx11.h"

namespace rePlayer
{
    inline auto GraphicsImpl::GetDevice() const
    {
        return m_device.Get();
    }
}
// namespace rePlayer

#endif // _WIN64