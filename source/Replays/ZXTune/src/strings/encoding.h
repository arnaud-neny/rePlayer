/**
 *
 * @file
 *
 * @brief  Encoding-related
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common included
#include <types.h>

namespace Strings
{
  String ToAutoUtf8(StringView str);

  String Utf16ToUtf8(basic_string_view<uint16_t> str);
}  // namespace Strings
