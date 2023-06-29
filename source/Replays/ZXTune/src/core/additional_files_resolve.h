/**
 *
 * @file
 *
 * @brief  Additional files resolving basic algorithm interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <module/additional_files.h>

namespace Module
{
  class AdditionalFilesSource
  {
  public:
    virtual ~AdditionalFilesSource() = default;

    //! @throws Error
    virtual Binary::Container::Ptr Get(const String& name) const = 0;
  };

  //! @throws Error
  void ResolveAdditionalFiles(const AdditionalFilesSource& source, const AdditionalFiles& target);
}  // namespace Module
