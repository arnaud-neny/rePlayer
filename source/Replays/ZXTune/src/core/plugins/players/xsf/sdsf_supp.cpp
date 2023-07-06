/**
 *
 * @file
 *
 * @brief  SSF/DSF support plugin
 *
 * @author vitamin.caig@gmail.com
 *
 **/
// local includes
#include "core/plugins/player_plugins_registrator.h"
#include "core/plugins/players/plugin.h"
// library includes
#include <core/plugin_attrs.h>
#include <formats/chiptune/decoders.h>
#include <module/players/xsf/sdsf.h>

namespace ZXTune
{
  void RegisterSDSFSupport(PlayerPluginsRegistrator& registrator)
  {
    const uint_t CAPS = Capabilities::Module::Type::MEMORYDUMP | Capabilities::Module::Device::DAC
                        | Capabilities::Module::Traits::MULTIFILE;
    const auto factory = Module::SDSF::CreateFactory();
    {
      auto decoder = Formats::Chiptune::CreateSSFDecoder();
      auto plugin = CreatePlayerPlugin("SSF"_id, CAPS, std::move(decoder), factory);
      registrator.RegisterPlugin(std::move(plugin));
    }
    {
      auto decoder = Formats::Chiptune::CreateDSFDecoder();
      auto plugin = CreatePlayerPlugin("DSF"_id, CAPS, std::move(decoder), factory);
      registrator.RegisterPlugin(std::move(plugin));
    }
  }
}  // namespace ZXTune
