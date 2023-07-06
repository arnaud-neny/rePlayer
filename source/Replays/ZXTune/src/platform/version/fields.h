/**
 *
 * @file
 *
 * @brief Version fields factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <strings/fields.h>

namespace Platform::Version
{
  std::unique_ptr<Strings::FieldsSource> CreateVersionFieldsSource();
}  // namespace Platform::Version
