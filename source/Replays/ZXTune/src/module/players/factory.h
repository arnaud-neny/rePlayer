/**
 *
 * @file
 *
 * @brief  Module factory interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <binary/container.h>
#include <module/holder.h>
#include <parameters/container.h>

namespace Module
{
  template<class ContainerType>
  class BaseFactory
  {
  public:
    using Ptr = std::shared_ptr<const BaseFactory>;
    virtual ~BaseFactory() = default;

    virtual Holder::Ptr CreateModule(const Parameters::Accessor& params, const ContainerType& data,
                                     Parameters::Container::Ptr properties) const = 0;
  };

  using Factory = BaseFactory<Binary::Container>;
}  // namespace Module
