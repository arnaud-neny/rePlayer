#pragma once

#include <Core.h>

namespace core::Hash
{
    /**
     * Get / hash calculator implementations
     */
    template <typename T>
    uint32_t Get(const T& data);
    uint32_t Get(const void* data, size_t size, uint32_t seed = 0);

    /**
     * Compare / compare functions for hashed types
     */
    template<typename T0, typename T1>
    bool Compare(const T0& a, const T1& b);
}
// namespace core::Hash

#include "HashTypes.inl.h"