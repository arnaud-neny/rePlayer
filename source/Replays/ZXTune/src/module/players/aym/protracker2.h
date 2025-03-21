/**
 *
 * @file
 *
 * @brief  ProTracker2 chiptune factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#include "module/players/aym/aym_factory.h"

namespace Module::ProTracker2
{
  AYM::Factory::Ptr CreateFactory();
}  // namespace Module::ProTracker2
