/**
 *
 * @file
 *
 * @brief  Multitrack player plugin factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// local includes
#include "core/plugins/archive_plugin.h"
#include "core/plugins/player_plugin.h"
// library includes
#include <formats/multitrack.h>
#include <module/players/factory.h>

namespace Module
{
  using MultitrackFactory = BaseFactory<Formats::Multitrack::Container>;
}

namespace ZXTune
{
  PlayerPlugin::Ptr CreatePlayerPlugin(PluginId id, uint_t caps, Formats::Multitrack::Decoder::Ptr decoder,
                                       Module::MultitrackFactory::Ptr factory);
  ArchivePlugin::Ptr CreateArchivePlugin(PluginId id, Formats::Multitrack::Decoder::Ptr decoder,
                                         Module::MultitrackFactory::Ptr factory);
}  // namespace ZXTune
