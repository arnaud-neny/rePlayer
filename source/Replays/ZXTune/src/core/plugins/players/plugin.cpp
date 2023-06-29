/**
 *
 * @file
 *
 * @brief  Player plugin implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "core/plugins/players/plugin.h"
// common includes
#include <make_ptr.h>
// library includes
#include <core/module_detect.h>
#include <core/plugin_attrs.h>
#include <module/attributes.h>
#include <module/players/properties_helper.h>
// std includes
#include <utility>

namespace ZXTune
{
  class CommonPlayerPlugin : public PlayerPlugin
  {
  public:
    CommonPlayerPlugin(StringView id, uint_t caps, Formats::Chiptune::Decoder::Ptr decoder,
                       Module::Factory::Ptr factory)
      : Identifier(id.to_string())
      , Caps(caps)
      , Decoder(std::move(decoder))
      , Factory(std::move(factory))
    {}

    String Id() const override
    {
      return Identifier;
    }

    String Description() const override
    {
      return Decoder->GetDescription();
    }

    uint_t Capabilities() const override
    {
      return Caps;
    }

    Binary::Format::Ptr GetFormat() const override
    {
      return Decoder->GetFormat();
    }

    Analysis::Result::Ptr Detect(const Parameters::Accessor& params, DataLocation::Ptr inputData,
                                 Module::DetectCallback& callback) const override
    {
      auto data = inputData->GetData();
      if (Decoder->Check(*data))
      {
        auto properties = callback.CreateInitialProperties(inputData->GetPath()->AsString());
        Module::PropertiesHelper props(*properties);
        props.SetContainer(inputData->GetPluginsChain()->AsString());
        if (auto holder = Factory->CreateModule(params, *data, properties))
        {
          props.SetType(Identifier);
          callback.ProcessModule(*inputData, *this, std::move(holder));
          Parameters::IntType usedSize = 0;
          properties->FindValue(Module::ATTR_SIZE, usedSize);
          return Analysis::CreateMatchedResult(static_cast<std::size_t>(usedSize));
        }
      }
      return Analysis::CreateUnmatchedResult(Decoder->GetFormat(), std::move(data));
    }

    Module::Holder::Ptr TryOpen(const Parameters::Accessor& params, const Binary::Container& data,
                                Parameters::Container::Ptr properties) const override
    {
      if (Decoder->Check(data))
      {
        if (auto result = Factory->CreateModule(params, data, properties))
        {
          Module::PropertiesHelper(*properties).SetType(Identifier);
          return result;
        }
      }
      return {};
    }

  private:
    const String Identifier;
    const uint_t Caps;
    const Formats::Chiptune::Decoder::Ptr Decoder;
    const Module::Factory::Ptr Factory;
  };

  PlayerPlugin::Ptr CreatePlayerPlugin(StringView id, uint_t caps, Formats::Chiptune::Decoder::Ptr decoder,
                                       Module::Factory::Ptr factory)
  {
    return MakePtr<CommonPlayerPlugin>(id, caps | Capabilities::Category::MODULE, std::move(decoder),
                                       std::move(factory));
  }

  class ExternalParsingPlayerPlugin : public PlayerPlugin
  {
  public:
    ExternalParsingPlayerPlugin(StringView id, uint_t caps, Formats::Chiptune::Decoder::Ptr decoder,
                                Module::ExternalParsingFactory::Ptr factory)
      : Identifier(id.to_string())
      , Caps(caps)
      , Decoder(std::move(decoder))
      , Factory(std::move(factory))
    {}

    String Id() const override
    {
      return Identifier;
    }

    String Description() const override
    {
      return Decoder->GetDescription();
    }

    uint_t Capabilities() const override
    {
      return Caps;
    }

    Binary::Format::Ptr GetFormat() const override
    {
      return Decoder->GetFormat();
    }

    Analysis::Result::Ptr Detect(const Parameters::Accessor& params, DataLocation::Ptr inputData,
                                 Module::DetectCallback& callback) const override
    {
      auto data = inputData->GetData();
      if (const auto container = Decoder->Decode(*data))
      {
        auto properties = callback.CreateInitialProperties(inputData->GetPath()->AsString());
        Module::PropertiesHelper props(*properties);
        props.SetContainer(inputData->GetPluginsChain()->AsString());
        if (auto holder = Factory->CreateModule(params, *container, properties))
        {
          props.SetSource(*container);
          props.SetType(Identifier);
          callback.ProcessModule(*inputData, *this, std::move(holder));
          return Analysis::CreateMatchedResult(container->Size());
        }
      }
      return Analysis::CreateUnmatchedResult(Decoder->GetFormat(), std::move(data));
    }

    Module::Holder::Ptr TryOpen(const Parameters::Accessor& params, const Binary::Container& data,
                                Parameters::Container::Ptr properties) const override
    {
      if (const auto container = Decoder->Decode(data))
      {
        if (auto result = Factory->CreateModule(params, *container, properties))
        {
          Module::PropertiesHelper props(*properties);
          props.SetSource(*container);
          props.SetType(Identifier);
          return result;
        }
      }
      return {};
    }

  private:
    const String Identifier;
    const uint_t Caps;
    const Formats::Chiptune::Decoder::Ptr Decoder;
    const Module::ExternalParsingFactory::Ptr Factory;
  };

  PlayerPlugin::Ptr CreatePlayerPlugin(StringView id, uint_t caps, Formats::Chiptune::Decoder::Ptr decoder,
                                       Module::ExternalParsingFactory::Ptr factory)
  {
    return MakePtr<ExternalParsingPlayerPlugin>(id, caps | Capabilities::Category::MODULE, std::move(decoder),
                                                std::move(factory));
  }
}  // namespace ZXTune
