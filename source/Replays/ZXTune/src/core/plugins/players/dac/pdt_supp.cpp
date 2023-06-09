/**
 *
 * @file
 *
 * @brief  ProDigiTracker support plugin
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "core/plugins/player_plugins_registrator.h"
#include "core/plugins/players/dac/dac_plugin.h"
// library includes
#include <core/plugin_attrs.h>
#include <formats/chiptune/digital/prodigitracker.h>
#include <module/players/dac/prodigitracker.h>

namespace ZXTune
{
  void RegisterPDTSupport(PlayerPluginsRegistrator& registrator)
  {
    auto decoder = Formats::Chiptune::CreateProDigiTrackerDecoder();
    auto factory = Module::ProDigiTracker::CreateFactory();
    auto plugin = CreatePlayerPlugin("PDT"_id, std::move(decoder), std::move(factory));
    registrator.RegisterPlugin(std::move(plugin));
  }
}  // namespace ZXTune
