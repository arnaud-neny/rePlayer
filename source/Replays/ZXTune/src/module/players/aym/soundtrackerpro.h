/**
 *
 * @file
 *
 * @brief  SoundTrackerPro-based modules support
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// local includes
#include "module/players/aym/aym_factory.h"
// library includes
#include <formats/chiptune/aym/soundtrackerpro.h>

namespace Module
{
  namespace SoundTrackerPro
  {
    AYM::Factory::Ptr CreateFactory(Formats::Chiptune::SoundTrackerPro::Decoder::Ptr decoder);
  }
}  // namespace Module
