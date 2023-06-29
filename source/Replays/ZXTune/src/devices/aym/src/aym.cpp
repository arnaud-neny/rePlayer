/**
 *
 * @file
 *
 * @brief  AY/YM-based chip implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "devices/aym/src/psg.h"
#include "devices/aym/src/soundchip.h"
// common includes
#include <make_ptr.h>
// std includes
#include <utility>

namespace Devices::AYM
{
  static_assert(Registers::TOTAL == 14, "Invalid registers count");
  static_assert(sizeof(Registers) == 16, "Invalid layout");

  struct Traits
  {
    typedef DataChunk DataChunkType;
    typedef PSG PSGType;
    typedef Chip ChipBaseType;
    static const uint_t VOICES = AYM::VOICES;
  };

  Chip::Ptr CreateChip(ChipParameters::Ptr params, MixerType::Ptr mixer)
  {
    return MakePtr<SoundChip<Traits> >(std::move(params), std::move(mixer));
  }
}  // namespace Devices::AYM
