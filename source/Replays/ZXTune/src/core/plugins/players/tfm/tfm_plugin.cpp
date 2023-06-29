/**
 *
 * @file
 *
 * @brief  TFM-based player plugin factory
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "core/plugins/players/tfm/tfm_plugin.h"
#include "core/plugins/players/plugin.h"
// common includes
#include <make_ptr.h>
// library includes
#include <core/plugin_attrs.h>
#include <module/players/tfm/tfm_base.h>
#include <module/players/tfm/tfm_parameters.h>
// std includes
#include <utility>

namespace Module
{
  class TFMHolder : public Holder
  {
  public:
    explicit TFMHolder(TFM::Chiptune::Ptr chiptune)
      : Tune(std::move(chiptune))
    {}

    Information::Ptr GetModuleInformation() const override
    {
      return Tune->GetInformation();
    }

    Parameters::Accessor::Ptr GetModuleProperties() const override
    {
      return Tune->GetProperties();
    }

    Renderer::Ptr CreateRenderer(uint_t samplerate, Parameters::Accessor::Ptr params) const override
    {
      auto chipParams = TFM::CreateChipParameters(samplerate, std::move(params));
      auto chip = Devices::TFM::CreateChip(std::move(chipParams));
      auto iterator = Tune->CreateDataIterator();
      return TFM::CreateRenderer(Tune->GetFrameDuration() /*TODO: speed variation*/, std::move(iterator),
                                 std::move(chip));
    }

  private:
    const TFM::Chiptune::Ptr Tune;
  };

  class TFMFactory : public Factory
  {
  public:
    explicit TFMFactory(TFM::Factory::Ptr delegate)
      : Delegate(std::move(delegate))
    {}

    Holder::Ptr CreateModule(const Parameters::Accessor& /*params*/, const Binary::Container& data,
                             Parameters::Container::Ptr properties) const override
    {
      if (auto chiptune = Delegate->CreateChiptune(data, std::move(properties)))
      {
        return MakePtr<TFMHolder>(std::move(chiptune));
      }
      else
      {
        return {};
      }
    }

  private:
    const TFM::Factory::Ptr Delegate;
  };
}  // namespace Module

namespace ZXTune
{
  PlayerPlugin::Ptr CreatePlayerPlugin(const String& id, uint_t caps, Formats::Chiptune::Decoder::Ptr decoder,
                                       Module::TFM::Factory::Ptr factory)
  {
    const Module::Factory::Ptr modFactory = MakePtr<Module::TFMFactory>(factory);
    const uint_t tfmCaps = Capabilities::Module::Device::TURBOFM;
    return CreatePlayerPlugin(id, caps | tfmCaps, decoder, modFactory);
  }

  PlayerPlugin::Ptr CreateTrackPlayerPlugin(const String& id, Formats::Chiptune::Decoder::Ptr decoder,
                                            Module::TFM::Factory::Ptr factory)
  {
    return CreatePlayerPlugin(id, Capabilities::Module::Type::TRACK, decoder, factory);
  }

  PlayerPlugin::Ptr CreateStreamPlayerPlugin(const String& id, Formats::Chiptune::Decoder::Ptr decoder,
                                             Module::TFM::Factory::Ptr factory)
  {
    return CreatePlayerPlugin(id, Capabilities::Module::Type::STREAM, decoder, factory);
  }
}  // namespace ZXTune
