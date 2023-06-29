/**
 *
 * @file
 *
 * @brief  SQTracker support plugin
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "core/plugins/player_plugins_registrator.h"
#include "core/plugins/players/ay/aym_plugin.h"
// library includes
#include <formats/chiptune/aym/sqtracker.h>
#include <module/players/aym/sqtracker.h>

namespace ZXTune
{
  void RegisterSQTSupport(PlayerPluginsRegistrator& registrator)
  {
    // plugin attributes
    const Char ID[] = {'S', 'Q', 'T', 0};

    const Formats::Chiptune::Decoder::Ptr decoder = Formats::Chiptune::CreateSQTrackerDecoder();
    const Module::AYM::Factory::Ptr factory = Module::SQTracker::CreateFactory();
    const PlayerPlugin::Ptr plugin = CreateTrackPlayerPlugin(ID, decoder, factory);
    registrator.RegisterPlugin(plugin);
  }
}  // namespace ZXTune
