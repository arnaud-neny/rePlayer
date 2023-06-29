/**
 *
 * @file
 *
 * @brief  TFM parameters helpers implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "module/players/tfm/tfm_parameters.h"
// common includes
#include <make_ptr.h>
// library includes
#include <core/core_parameters.h>
// std includes
#include <utility>

namespace Module::TFM
{
  class ChipParameters : public Devices::TFM::ChipParameters
  {
  public:
    ChipParameters(uint_t samplerate, Parameters::Accessor::Ptr params)
      : Samplerate(samplerate)
      , Params(params)
    {}

    uint_t Version() const override
    {
      return Params->Version();
    }

    uint64_t ClockFreq() const override
    {
      Parameters::IntType val = Parameters::ZXTune::Core::FM::CLOCKRATE_DEFAULT;
      Params->FindValue(Parameters::ZXTune::Core::FM::CLOCKRATE, val);
      return val;
    }

    uint_t SoundFreq() const override
    {
      return Samplerate;
    }

  private:
    const uint_t Samplerate;
    const Parameters::Accessor::Ptr Params;
  };

  Devices::TFM::ChipParameters::Ptr CreateChipParameters(uint_t samplerate, Parameters::Accessor::Ptr params)
  {
    return MakePtr<ChipParameters>(samplerate, std::move(params));
  }
}  // namespace Module::TFM
