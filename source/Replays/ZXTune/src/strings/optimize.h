/**
 *
 * @file
 *
 * @brief  String optimization
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>

namespace Strings
{
  // trim all the non-ascii symbols from begin/end and replace by replacement at the middle
  String OptimizeAscii(StringView str, Char replacement);

  inline String OptimizeAscii(StringView str)
  {
    return OptimizeAscii(str, '\?');
  }
}  // namespace Strings
