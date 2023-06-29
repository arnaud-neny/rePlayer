/**
 *
 * @file
 *
 * @brief  Analyzer interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>
// library includes
#include <math/fixedpoint.h>
// std includes
#include <memory>

namespace Sound
{
  //! @brief %Sound analyzer interface
  class Analyzer
  {
  public:
    //! Pointer type
    typedef std::shared_ptr<const Analyzer> Ptr;

    virtual ~Analyzer() = default;

    using LevelType = Math::FixedPoint<uint8_t, 100>;

    // TODO: use std::span when available
    virtual void GetSpectrum(LevelType* result, std::size_t limit) const = 0;
  };
}  // namespace Sound
