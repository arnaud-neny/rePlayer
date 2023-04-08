#pragma once

#include "Countries.h"

namespace rePlayer
{
    inline uint16_t Countries::GetCode(int32_t index)
    {
        return ms_countriesId[index];
    }
}
// namespace rePlayer