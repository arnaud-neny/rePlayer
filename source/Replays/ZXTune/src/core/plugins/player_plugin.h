/**
 *
 * @file
 *
 * @brief  PlayerPlugin interface declaration
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// local includes
#include "core/src/location.h"
// library includes
#include <analysis/result.h>
#include <core/plugin.h>
#include <module/holder.h>
#include <parameters/container.h>

namespace Module
{
  class DetectCallback;
}

namespace ZXTune
{
  class PlayerPlugin : public Plugin
  {
  public:
    typedef std::shared_ptr<const PlayerPlugin> Ptr;

    static const std::vector<Ptr>& Enumerate();

    virtual Binary::Format::Ptr GetFormat() const = 0;

    //! @brief Detect modules in data
    virtual Analysis::Result::Ptr Detect(const Parameters::Accessor& params, DataLocation::Ptr inputData,
                                         Module::DetectCallback& callback) const = 0;

    virtual Module::Holder::Ptr TryOpen(const Parameters::Accessor& params, const Binary::Container& data,
                                        Parameters::Container::Ptr initialProperties) const = 0;
  };
}  // namespace ZXTune
