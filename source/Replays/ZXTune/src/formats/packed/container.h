/**
 *
 * @file
 *
 * @brief  Packed data container helper
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>
// library includes
#include <binary/dump.h>
#include <formats/packed.h>

namespace Formats
{
  namespace Packed
  {
    Container::Ptr CreateContainer(Binary::Container::Ptr data, std::size_t origSize);
    Container::Ptr CreateContainer(std::unique_ptr<Binary::Dump> data, std::size_t origSize);
  }  // namespace Packed
}  // namespace Formats
