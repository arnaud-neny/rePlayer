/**
 *
 * @file
 *
 * @brief  Module::Holder extension for additional files operating
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <binary/container.h>
#include <strings/array.h>

namespace Module
{
  class AdditionalFiles
  {
  public:
    virtual Strings::Array Enumerate() const = 0;
    virtual void Resolve(const String& name, Binary::Container::Ptr data) = 0;
  };
}  // namespace Module
