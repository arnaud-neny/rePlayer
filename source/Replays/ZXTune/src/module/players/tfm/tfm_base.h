/**
 *
 * @file
 *
 * @brief  TFM-based chiptunes support
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#include "module/players/tfm/tfm_chiptune.h"

#include "module/renderer.h"

namespace Module::TFM
{
  Renderer::Ptr CreateRenderer(Time::Microseconds frameDuration, DataIterator::Ptr iterator,
                               Devices::TFM::Chip::Ptr device);
}  // namespace Module::TFM
