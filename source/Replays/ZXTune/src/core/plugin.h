/**
 *
 * @file
 *
 * @brief  Plugins related interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>

namespace ZXTune
{
  //! @brief Basic plugin interface
  class Plugin
  {
  public:
    virtual ~Plugin() = default;

    //! Identification string
    virtual String Id() const = 0;
    //! Textual description
    virtual String Description() const = 0;
    //! Plugin capabilities @see plugin_attrs.h
    virtual uint_t Capabilities() const = 0;
  };

  class PluginVisitor
  {
  public:
    virtual ~PluginVisitor() = default;

    virtual void Visit(const Plugin& plugin) = 0;
  };

  void EnumeratePlugins(PluginVisitor& visitor);
}  // namespace ZXTune