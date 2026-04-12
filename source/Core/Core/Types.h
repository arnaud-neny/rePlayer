#pragma once

#include <assert.h>
#include <stdint.h>
#include <utility>

#ifndef NOMINMAX
#   define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif

namespace core
{
    // basic enum to force definition of some functions (see SmartPtr(AutoAllocate...))
    enum AutoAllocate
    {
        kAllocate
    };

    // return codes (< kOk as error, > kOk as warning)
    enum class Status
    {
        kFail = -1,
        kOk = 0
    };

    template <typename TOut, typename TIn>
    inline TOut gCast(TIn&& data)
    {
        return TOut(std::forward<TIn>(data));
    }

    template <typename TOut, typename TIn>
    inline TOut cCast(TIn&& data)
    {
        return TOut(std::forward<TIn>(data));
    }

    template <typename TOut, typename TIn>
    inline TOut* pCast(TIn* data)
    {
        return reinterpret_cast<TOut*>(data);
    }

    template <typename TOut, typename TIn>
    inline const TOut* pcCast(const TIn* data)
    {
        return reinterpret_cast<const TOut*>(data);
    }

    template <typename TOut, typename TIn>
    inline TOut& rCast(TIn& data)
    {
        return reinterpret_cast<TOut&>(data);
    }

    template <typename TOut, typename TIn>
    inline const TOut& rcCast(const TIn& data)
    {
        return reinterpret_cast<const TOut&>(data);
    }
}
// namespace core