#pragma once

#include "GraphicsDx11.h"

namespace rePlayer
{
    inline auto GraphicsDX11::GetDevice() const
    {
        return m_device.Get();
    }
}
// namespace rePlayer