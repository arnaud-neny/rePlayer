/**
 *
 * @file
 *
 * @brief  ExtremeTracker v1 chiptune factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// local includes
#include "module/players/dac/dac_factory.h"

namespace Module
{
  namespace ExtremeTracker1
  {
    DAC::Factory::Ptr CreateFactory();
  }
}  // namespace Module
