/**
 *
 * @file
 *
 * @brief  SSF/DSF chiptune factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#include "module/players/xsf/xsf_factory.h"

namespace Module::SDSF
{
  XSF::Factory::Ptr CreateFactory();
}  // namespace Module::SDSF
