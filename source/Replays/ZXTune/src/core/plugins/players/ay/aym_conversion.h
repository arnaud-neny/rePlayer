/**
 *
 * @file
 *
 * @brief  AYM-base conversion details
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>
// library includes
#include <core/plugin_attrs.h>

namespace Module
{
  namespace AYM
  {
    inline uint_t GetSupportedFormatConvertors()
    {
      using namespace ZXTune::Capabilities::Module;
      return Conversion::PSG | Conversion::ZX50 | Conversion::AYDUMP | Conversion::FYM;
    }
  }  // namespace AYM

  namespace Vortex
  {
    inline uint_t GetSupportedFormatConvertors()
    {
      return 0;
    }
  }  // namespace Vortex
}  // namespace Module
