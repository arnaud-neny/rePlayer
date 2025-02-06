/**
 *
 * @file
 *
 * @brief  DigitalStudio chiptune factory implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#include "module/players/dac/digitalstudio.h"

#include "formats/chiptune/digital/digitalstudio.h"
#include "module/players/dac/dac_simple.h"

#include "make_ptr.h"

namespace Module::DigitalStudio
{
  const std::size_t CHANNELS_COUNT = 3;

  using ModuleData = DAC::SimpleModuleData;
  using DataBuilder = DAC::SimpleDataBuilder;

  class Factory : public DAC::Factory
  {
  public:
    DAC::Chiptune::Ptr CreateChiptune(const Binary::Container& rawData,
                                      Parameters::Container::Ptr properties) const override
    {
      DAC::PropertiesHelper props(*properties, CHANNELS_COUNT);
      DataBuilder::Ptr dataBuilder = DAC::CreateSimpleDataBuilder<CHANNELS_COUNT>(props);
      if (const auto container = Formats::Chiptune::DigitalStudio::Parse(rawData, *dataBuilder))
      {
        props.SetSource(*container);
        return DAC::CreateSimpleChiptune(dataBuilder->CaptureResult(), std::move(properties));
      }
      else
      {
        return {};
      }
    }
  };

  Factory::Ptr CreateFactory()
  {
    return MakePtr<Factory>();
  }
}  // namespace Module::DigitalStudio
