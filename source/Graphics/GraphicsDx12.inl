#pragma once

#ifdef _WIN64
#include "GraphicsDx12.h"

namespace rePlayer
{
    inline auto GraphicsDX12::GetDevice() const
    {
        return m_device.Get();
    }
}
// namespace rePlayer

#endif // _WIN64