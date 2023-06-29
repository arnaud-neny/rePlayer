/**
 *
 * @file
 *
 * @brief  Chiptune container helper
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// library includes
#include <formats/chiptune.h>

namespace Formats
{
  namespace Chiptune
  {
    Container::Ptr CreateKnownCrcContainer(Binary::Container::Ptr data, uint_t crc);
    Container::Ptr CreateCalculatingCrcContainer(Binary::Container::Ptr data, std::size_t offset, std::size_t size);
  }  // namespace Chiptune
}  // namespace Formats
