/**
 *
 * @file
 *
 * @brief  YM/VTX chiptune factory implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "module/players/aym/ymvtx.h"
#include "module/players/aym/aym_base.h"
#include "module/players/aym/aym_base_stream.h"
#include "module/players/aym/aym_properties_helper.h"
// common includes
#include <make_ptr.h>
// library includes
#include <core/core_parameters.h>
// std includes
#include <utility>
// boost includes
#include <boost/lexical_cast.hpp>

namespace Module::YMVTX
{
  Devices::AYM::LayoutType VtxMode2AymLayout(uint_t mode)
  {
    using namespace Devices::AYM;
    switch (mode)
    {
    case 0:
      return LAYOUT_MONO;
    case 1:
      return LAYOUT_ABC;
    case 2:
      return LAYOUT_ACB;
    case 3:
      return LAYOUT_BAC;
    case 4:
      return LAYOUT_BCA;
    case 5:
      return LAYOUT_CAB;
    case 6:
      return LAYOUT_CBA;
    default:
      assert(!"Invalid VTX layout");
      return LAYOUT_ABC;
    }
  }

  class DataBuilder : public Formats::Chiptune::YM::Builder
  {
  public:
    explicit DataBuilder(AYM::PropertiesHelper& props)
      : Properties(props)
      , Data(MakePtr<AYM::MutableStreamModel>())
    {}

    void SetVersion(const String& version) override
    {
      Properties.SetVersion(version);
    }

    void SetChipType(bool ym) override
    {
      Properties.SetChipType(ym ? Parameters::ZXTune::Core::AYM::TYPE_YM : Parameters::ZXTune::Core::AYM::TYPE_AY);
    }

    void SetStereoMode(uint_t mode) override
    {
      Properties.SetChannelsLayout(VtxMode2AymLayout(mode));
    }

    void SetLoop(uint_t loop) override
    {
      Data->SetLoop(loop);
    }

    void SetDigitalSample(uint_t /*idx*/, const Binary::Dump& /*data*/) override
    {
      // TODO:
    }

    void SetClockrate(uint64_t freq) override
    {
      Properties.SetChipFrequency(freq);
    }

    void SetIntFreq(uint_t freq) override
    {
      if (freq)
      {
        FrameDuration = Time::Microseconds::FromFrequency(freq);
      }
    }

    void SetTitle(const String& title) override
    {
      Properties.SetTitle(title);
    }

    void SetAuthor(const String& author) override
    {
      Properties.SetAuthor(author);
    }

    void SetComment(const String& comment) override
    {
      Properties.SetComment(comment);
    }

    void SetYear(uint_t year) override
    {
      if (year)
      {
        Properties.SetDate(boost::lexical_cast<String>(year));
      }
    }

    void SetProgram(const String& /*program*/) override
    {
      // TODO
    }

    void SetEditor(const String& editor) override
    {
      Properties.SetProgram(editor);
    }

    void AddData(const Binary::Dump& registers) override
    {
      auto& data = Data->AddFrame();
      const uint_t availRegs = std::min<uint_t>(registers.size(), Devices::AYM::Registers::ENV + 1);
      for (uint_t reg = 0, mask = 1; reg != availRegs; ++reg, mask <<= 1)
      {
        const uint8_t val = registers[reg];
        if (reg != Devices::AYM::Registers::ENV || val != 0xff)
        {
          data[static_cast<Devices::AYM::Registers::Index>(reg)] = val;
        }
      }
    }

    AYM::StreamModel::Ptr CaptureResult() const
    {
      return Data->IsEmpty() ? AYM::StreamModel::Ptr() : AYM::StreamModel::Ptr(std::move(Data));
    }

    Time::Microseconds GetFrameDuration() const
    {
      return FrameDuration;
    }

  private:
    AYM::PropertiesHelper& Properties;
    AYM::MutableStreamModel::Ptr Data;
    Time::Microseconds FrameDuration = AYM::BASE_FRAME_DURATION;
  };

  class Factory : public AYM::Factory
  {
  public:
    explicit Factory(Formats::Chiptune::YM::Decoder::Ptr decoder)
      : Decoder(std::move(decoder))
    {}

    AYM::Chiptune::Ptr CreateChiptune(const Binary::Container& rawData,
                                      Parameters::Container::Ptr properties) const override
    {
      AYM::PropertiesHelper props(*properties);
      DataBuilder dataBuilder(props);
      if (const auto container = Decoder->Parse(rawData, dataBuilder))
      {
        if (auto data = dataBuilder.CaptureResult())
        {
          props.SetSource(*container);
          return AYM::CreateStreamedChiptune(dataBuilder.GetFrameDuration(), std::move(data), std::move(properties));
        }
      }
      return {};
    }

  private:
    const Formats::Chiptune::YM::Decoder::Ptr Decoder;
  };

  Factory::Ptr CreateFactory(Formats::Chiptune::YM::Decoder::Ptr decoder)
  {
    return MakePtr<Factory>(std::move(decoder));
  }
}  // namespace Module::YMVTX
