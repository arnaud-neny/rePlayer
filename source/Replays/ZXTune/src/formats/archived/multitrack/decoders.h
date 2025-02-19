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

#include "formats/archived.h"

namespace Formats::Archived
{
  Decoder::Ptr CreateAYDecoder();
}  // namespace Formats::Archived
