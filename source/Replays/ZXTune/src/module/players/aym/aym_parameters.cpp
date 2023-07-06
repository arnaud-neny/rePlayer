/**
 *
 * @file
 *
 * @brief  AYM parameters helpers implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "module/players/aym/aym_parameters.h"
#include "core/plugins/players/ay/freq_tables_internal.h"
// common includes
#include <contract.h>
#include <error_tools.h>
#include <make_ptr.h>
// library includes
#include <core/core_parameters.h>
#include <devices/aym/chip.h>
#include <l10n/api.h>
#include <math/numeric.h>
// std includes
#include <cstring>
#include <numeric>
#include <utility>

namespace Module::AYM
{
  const L10n::TranslateFunctor translate = L10n::TranslateFunctor("module_players");

  // duty-cycle related parameter: accumulate letters to bitmask functor
  inline uint_t LetterToMask(uint_t val, const Char letter)
  {
    static const Char LETTERS[] = {'A', 'B', 'C'};
    static const uint_t MASKS[] = {
        Devices::AYM::CHANNEL_MASK_A,
        Devices::AYM::CHANNEL_MASK_B,
        Devices::AYM::CHANNEL_MASK_C,
    };
    static_assert(sizeof(LETTERS) / sizeof(*LETTERS) == sizeof(MASKS) / sizeof(*MASKS), "Invalid layout");
    const Char* const pos = std::find(LETTERS, std::end(LETTERS), letter);
    if (pos == std::end(LETTERS))
    {
      throw MakeFormattedError(THIS_LINE, translate("Invalid duty cycle mask item: '{}'."), String(1, letter));
    }
    return val | MASKS[pos - LETTERS];
  }

  uint_t String2Mask(StringView str)
  {
    return std::accumulate(str.begin(), str.end(), uint_t(0), LetterToMask);
  }

  Devices::AYM::LayoutType String2Layout(StringView str)
  {
    if (str == "ABC")
    {
      return Devices::AYM::LAYOUT_ABC;
    }
    else if (str == "ACB")
    {
      return Devices::AYM::LAYOUT_ACB;
    }
    else if (str == "BAC")
    {
      return Devices::AYM::LAYOUT_BAC;
    }
    else if (str == "BCA")
    {
      return Devices::AYM::LAYOUT_BCA;
    }
    else if (str == "CBA")
    {
      return Devices::AYM::LAYOUT_CBA;
    }
    else if (str == "CAB")
    {
      return Devices::AYM::LAYOUT_CAB;
    }
    else if (str == "MONO")
    {
      return Devices::AYM::LAYOUT_MONO;
    }
    else
    {
      throw MakeFormattedError(THIS_LINE, translate("Invalid layout value ({})."), str);
    }
  }

  class ChipParametersImpl : public Devices::AYM::ChipParameters
  {
  public:
    ChipParametersImpl(uint_t samplerate, Parameters::Accessor::Ptr params)
      : Samplerate(samplerate)
      , Params(std::move(params))
    {}

    uint_t Version() const override
    {
      return Params->Version();
    }

    uint64_t ClockFreq() const override
    {
      Parameters::IntType val = Parameters::ZXTune::Core::AYM::CLOCKRATE_DEFAULT;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::CLOCKRATE, val)
          && !Math::InRange(val, Parameters::ZXTune::Core::AYM::CLOCKRATE_MIN,
                            Parameters::ZXTune::Core::AYM::CLOCKRATE_MAX))
      {
        throw MakeFormattedError(THIS_LINE, translate("Invalid clock frequency ({})."), val);
      }
      return val;
    }

    uint_t SoundFreq() const override
    {
      return Samplerate;
    }

    Devices::AYM::ChipType Type() const override
    {
      Parameters::IntType intVal = Parameters::ZXTune::Core::AYM::TYPE_DEFAULT;
      Params->FindValue(Parameters::ZXTune::Core::AYM::TYPE, intVal);
      return static_cast<Devices::AYM::ChipType>(intVal);
    }

    Devices::AYM::InterpolationType Interpolation() const override
    {
      Parameters::IntType intVal = Parameters::ZXTune::Core::AYM::INTERPOLATION_DEFAULT;
      Params->FindValue(Parameters::ZXTune::Core::AYM::INTERPOLATION, intVal);
      return static_cast<Devices::AYM::InterpolationType>(intVal);
    }

    uint_t DutyCycleValue() const override
    {
      Parameters::IntType intVal = Parameters::ZXTune::Core::AYM::DUTY_CYCLE_DEFAULT;
      const bool found = Params->FindValue(Parameters::ZXTune::Core::AYM::DUTY_CYCLE, intVal);
      // duty cycle in percents should be in range 1..99 inc
      if (found
          && (intVal < Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MIN
              || intVal > Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MAX))
      {
        throw MakeFormattedError(THIS_LINE, translate("Invalid duty cycle value ({})."), intVal);
      }
      return static_cast<uint_t>(intVal);
    }

    uint_t DutyCycleMask() const override
    {
      Parameters::IntType intVal = Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MASK_DEFAULT;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MASK, intVal))
      {
        return static_cast<uint_t>(intVal);
      }
      Parameters::StringType strVal;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MASK, strVal))
      {
        return String2Mask(strVal);
      }
      return 0;
    }

    Devices::AYM::LayoutType Layout() const override
    {
      Parameters::IntType intVal = Parameters::ZXTune::Core::AYM::LAYOUT_DEFAULT;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::LAYOUT, intVal))
      {
        if (intVal < static_cast<int_t>(Devices::AYM::LAYOUT_ABC)
            || intVal >= static_cast<int_t>(Devices::AYM::LAYOUT_LAST))
        {
          throw MakeFormattedError(THIS_LINE, translate("Invalid layout value ({})."), intVal);
        }
        return static_cast<Devices::AYM::LayoutType>(intVal);
      }
      Parameters::StringType strVal;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::LAYOUT, strVal))
      {
        return String2Layout(strVal);
      }
      return Devices::AYM::LAYOUT_ABC;
    }

  private:
    const uint_t Samplerate;
    const Parameters::Accessor::Ptr Params;
  };

  class AYTrackParameters : public TrackParameters
  {
  public:
    explicit AYTrackParameters(Parameters::Accessor::Ptr params)
      : Params(std::move(params))
    {}

    uint_t Version() const override
    {
      return Params->Version();
    }

    void FreqTable(FrequencyTable& table) const override
    {
      Parameters::StringType newName;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::TABLE, newName))
      {
        GetFreqTable(newName, table);
      }
      else
      {
        Parameters::DataType newData;
        // frequency table is mandatory!!!
        Require(Params->FindValue(Parameters::ZXTune::Core::AYM::TABLE, newData));
        // as dump
        if (newData.size() != table.size() * sizeof(table.front()))
        {
          throw MakeFormattedError(THIS_LINE, translate("Invalid frequency table size ({})."), newData.size());
        }
        std::memcpy(table.data(), newData.data(), newData.size());
      }
    }

  private:
    const Parameters::Accessor::Ptr Params;
  };

  class TSTrackParameters : public TrackParameters
  {
  public:
    TSTrackParameters(Parameters::Accessor::Ptr params, uint_t idx)
      : Params(std::move(params))
      , Index(idx)
    {
      Require(Index <= 1);
    }

    uint_t Version() const override
    {
      return Params->Version();
    }

    void FreqTable(FrequencyTable& table) const override
    {
      Parameters::StringType newName;
      if (Params->FindValue(Parameters::ZXTune::Core::AYM::TABLE, newName))
      {
        const auto& subName = ExtractMergedValue(newName);
        GetFreqTable(subName, table);
      }
    }

  private:
    /*
      ('a', 0) => 'a'
      ('a', 1) => 'a'
      ('a/b', 0) => 'a'
      ('a/b', 1) => 'b'
    */
    StringView ExtractMergedValue(StringView val) const
    {
      const auto pos = val.find_first_of('/');
      if (pos != val.npos)
      {
        Require(val.npos == val.find_first_of('/', pos + 1));
        return Index == 0 ? val.substr(0, pos) : val.substr(pos + 1);
      }
      else
      {
        return val;
      }
    }

  private:
    const Parameters::Accessor::Ptr Params;
    const uint_t Index;
  };

  Devices::AYM::ChipParameters::Ptr CreateChipParameters(uint_t samplerate, Parameters::Accessor::Ptr params)
  {
    return MakePtr<ChipParametersImpl>(samplerate, std::move(params));
  }

  TrackParameters::Ptr TrackParameters::Create(Parameters::Accessor::Ptr params)
  {
    return MakePtr<AYTrackParameters>(std::move(params));
  }

  TrackParameters::Ptr TrackParameters::Create(Parameters::Accessor::Ptr params, uint_t idx)
  {
    return MakePtr<TSTrackParameters>(std::move(params), idx);
  }
}  // namespace Module::AYM
