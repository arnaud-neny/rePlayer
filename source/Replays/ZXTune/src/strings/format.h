/**
 *
 * @file
 *
 * @brief  Formatter type definition
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>
// std includes
#include <utility>
// other includes
#include <3rdparty/fmt/include/fmt/core.h>

namespace Strings
{
  //! @brief Format functions
  //! @code
  //! const String& txt = Strings::Format(FORMAT_STRING, param1, param2);
  //! @endcode
  template<class S, class... P>
  String Format(S&& format, P&&... params)
  {
    return fmt::vformat(std::forward<S>(format), fmt::make_format_args(std::forward<P>(params)...)); // rePlayer
  }

  String FormatTime(uint_t hours, uint_t minutes, uint_t seconds, uint_t frames);
}  // namespace Strings
