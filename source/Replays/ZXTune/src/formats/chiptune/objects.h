/**
 *
 * @file
 *
 * @brief  Object helpers
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>
// std includes
#include <type_traits>
#include <vector>

namespace Formats::Chiptune
{
  template<class LineType>
  class LinesObject
  {
  public:
    using Line = LineType;

    LinesObject() = default;

    LinesObject(const LinesObject&) = delete;
    LinesObject& operator=(const LinesObject&) = delete;

    LinesObject(LinesObject&& rh) noexcept = default;
    LinesObject& operator=(LinesObject&& rh) noexcept = default;

    uint_t GetLoop() const
    {
      return Loop;
    }

    uint_t GetSize() const
    {
      return static_cast<uint_t>(Lines.size());
    }

    typename std::conditional<std::is_fundamental<LineType>::value, LineType, const LineType&>::type
        GetLine(uint_t pos) const
    {
      static const LineType STUB = LineType();
      return Lines.size() > pos ? Lines[pos] : STUB;
    }

    const LineType* FindLine(uint_t pos) const
    {
      return Lines.size() > pos ? &Lines[pos] : nullptr;
    }

    std::vector<LineType> Lines;
    uint_t Loop = 0;
  };

  template<class LineType>
  class LinesObjectWithLoopLimit : public LinesObject<LineType>
  {
  public:
    LinesObjectWithLoopLimit()
      : LinesObject<LineType>()
    {}

    LinesObjectWithLoopLimit(LinesObjectWithLoopLimit&& rh) noexcept = default;
    LinesObjectWithLoopLimit& operator=(LinesObjectWithLoopLimit&& rh) noexcept = default;

    uint_t GetLoopLimit() const
    {
      return LoopLimit;
    }

    uint_t LoopLimit = 0;
  };
}  // namespace Formats::Chiptune
