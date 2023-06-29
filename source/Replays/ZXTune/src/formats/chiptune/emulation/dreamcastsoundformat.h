/**
 *
 * @file
 *
 * @brief  DSF parser interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <formats/chiptune.h>

namespace Formats
{
  namespace Chiptune
  {
    namespace DreamcastSoundFormat
    {
      const uint_t VERSION_ID = 0x12;
    }

    Decoder::Ptr CreateDSFDecoder();
  }  // namespace Chiptune
}  // namespace Formats
