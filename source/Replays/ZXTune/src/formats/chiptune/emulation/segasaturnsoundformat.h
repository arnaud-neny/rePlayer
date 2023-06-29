/**
 *
 * @file
 *
 * @brief  SSF parser interface
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
    namespace SegaSaturnSoundFormat
    {
      const uint_t VERSION_ID = 0x11;
    }

    Decoder::Ptr CreateSSFDecoder();
  }  // namespace Chiptune
}  // namespace Formats
