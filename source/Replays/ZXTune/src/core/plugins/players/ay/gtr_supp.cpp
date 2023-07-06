/**
 *
 * @file
 *
 * @brief  GlobalTracker support plugin
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "core/plugins/player_plugins_registrator.h"
#include "core/plugins/players/ay/aym_plugin.h"
// library includes
#include <formats/chiptune/aym/globaltracker.h>
#include <module/players/aym/globaltracker.h>

namespace ZXTune
{
  void RegisterGTRSupport(PlayerPluginsRegistrator& registrator)
  {
    auto decoder = Formats::Chiptune::CreateGlobalTrackerDecoder();
    auto factory = Module::GlobalTracker::CreateFactory();
    auto plugin = CreateTrackPlayerPlugin("GTR"_id, std::move(decoder), std::move(factory));
    registrator.RegisterPlugin(std::move(plugin));
  }
}  // namespace ZXTune
