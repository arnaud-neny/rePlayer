/**
 *
 * @file
 *
 * @brief  Packed data container helpers implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "formats/packed/container.h"
// common includes
#include <make_ptr.h>
// library includes
#include <binary/container_base.h>
#include <binary/container_factories.h>
// std includes
#include <cassert>

namespace Formats
{
  namespace Packed
  {
    class PackedContainer : public Binary::BaseContainer<Container>
    {
    public:
      PackedContainer(Binary::Container::Ptr delegate, std::size_t origSize)
        : BaseContainer(std::move(delegate))
        , OriginalSize(origSize)
      {}

      std::size_t PackedSize() const override
      {
        return OriginalSize;
      }

    private:
      const std::size_t OriginalSize;
    };

    Container::Ptr CreateContainer(Binary::Container::Ptr data, std::size_t origSize)
    {
      return origSize && data && data->Size() ? MakePtr<PackedContainer>(std::move(data), origSize) : Container::Ptr();
    }

    Container::Ptr CreateContainer(std::unique_ptr<Binary::Dump> data, std::size_t origSize)
    {
      auto container = Binary::CreateContainer(std::move(data));
      return CreateContainer(std::move(container), origSize);
    }
  }  // namespace Packed
}  // namespace Formats
