/**
 *
 * @file
 *
 * @brief  PSF chiptune factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#include "module/players/xsf/xsf_factory.h"

namespace Module::PSF
{
  XSF::Factory::Ptr CreateFactory();
}  // namespace Module::PSF
