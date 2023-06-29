/**
 *
 * @file
 *
 * @brief  Chiptune container helpers implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "formats/chiptune/container.h"
// common includes
#include <make_ptr.h>
// library includes
#include <binary/container_base.h>
#include <binary/crc.h>
// std includes
#include <cassert>

namespace Formats
{
  namespace Chiptune
  {
    class BaseDelegateContainer : public Binary::BaseContainer<Container>
    {
    public:
      explicit BaseDelegateContainer(Binary::Container::Ptr delegate)
        : BaseContainer(std::move(delegate))
      {}
    };

    class KnownCrcContainer : public BaseDelegateContainer
    {
    public:
      KnownCrcContainer(Binary::Container::Ptr delegate, uint_t crc)
        : BaseDelegateContainer(delegate)
        , Crc(crc)
      {}

      uint_t FixedChecksum() const override
      {
        return Crc;
      }

    private:
      const uint_t Crc;
    };

    class CalculatingCrcContainer : public BaseDelegateContainer
    {
    public:
      CalculatingCrcContainer(Binary::Container::Ptr delegate, std::size_t offset, std::size_t size)
        : BaseDelegateContainer(delegate)
        , FixedOffset(offset)
        , FixedSize(size)
      {}

      uint_t FixedChecksum() const override
      {
        return Binary::Crc32(Binary::View(*Delegate).SubView(FixedOffset, FixedSize));
      }

    private:
      const std::size_t FixedOffset;
      const std::size_t FixedSize;
    };

    Container::Ptr CreateKnownCrcContainer(Binary::Container::Ptr data, uint_t crc)
    {
      return data && data->Size() ? MakePtr<KnownCrcContainer>(data, crc) : Container::Ptr();
    }

    Container::Ptr CreateCalculatingCrcContainer(Binary::Container::Ptr data, std::size_t offset, std::size_t size)
    {
      return data && data->Size() ? MakePtr<CalculatingCrcContainer>(data, offset, size) : Container::Ptr();
    }
  }  // namespace Chiptune
}  // namespace Formats
