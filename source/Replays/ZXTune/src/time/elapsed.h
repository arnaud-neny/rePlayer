/**
 *
 * @file
 *
 * @brief  Elapsed time functor helper
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <time/duration.h>
// std includes
#include <ctime>
#include <type_traits>

namespace Time
{
  class Elapsed
  {
  public:
    using NativeUnit =
        BaseUnit<std::conditional<sizeof(std::clock_t) == sizeof(uint64_t), uint64_t, uint_t>::type, CLOCKS_PER_SEC>;

    template<class DurationType>
    explicit Elapsed(const DurationType& period)
      : Period(Duration<NativeUnit>(period).Get())
      , Next()
    {}

    bool operator()()
    {
      const clock_t current = ::clock();
      if (Next <= current)
      {
        Next = current + Period;
        return true;
      }
      else
      {
        return false;
      }
    }

  private:
    const clock_t Period;
    clock_t Next;
  };
}  // namespace Time
