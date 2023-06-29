/**
 *
 * @file
 *
 * @brief  Multitrack archives decoders factories
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <formats/archived.h>

namespace Formats
{
  namespace Archived
  {
    Decoder::Ptr CreateAYDecoder();
  }  // namespace Archived
}  // namespace Formats
