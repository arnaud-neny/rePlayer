#pragma once

#include "GraphicsImpl.h"

namespace rePlayer
{
    inline auto GraphicsImpl::GetDevice() const
    {
        return m_device.Get();
    }
}
// namespace rePlayer
