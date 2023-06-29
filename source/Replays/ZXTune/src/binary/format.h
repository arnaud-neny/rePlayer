/**
 *
 * @file
 *
 * @brief  Binary data format interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <binary/view.h>
// std includes
#include <memory>

namespace Binary
{
  //! Data format description
  class Format
  {
  public:
    typedef std::shared_ptr<const Format> Ptr;
    virtual ~Format() = default;

    //! @brief Check if input data is data format
    //! @param data Data to be checked
    //! @return true if data comply format
    virtual bool Match(View data) const = 0;
    //! @brief Search for matched offset in input data
    //! @param data Data to be checked
    //! @return Offset of matched data or size if not found
    //! @invariant return value is always > 0
    virtual std::size_t NextMatchOffset(View data) const = 0;
  };
}  // namespace Binary
