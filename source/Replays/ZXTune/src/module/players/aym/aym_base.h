/**
 *
 * @file
 *
 * @brief  AYM-based chiptunes support
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// local includes
#include "module/players/aym/aym_chiptune.h"
// library includes
#include <module/holder.h>

namespace Module
{
  namespace AYM
  {
    class Holder : public Module::Holder
    {
    public:
      typedef std::shared_ptr<const Holder> Ptr;

      using Module::Holder::CreateRenderer;
      virtual AYM::Chiptune::Ptr GetChiptune() const = 0;

      // TODO: move to another place
      virtual void Dump(Devices::AYM::Device& dev) const = 0;
    };

    Holder::Ptr CreateHolder(Chiptune::Ptr chiptune);

    Devices::AYM::Chip::Ptr CreateChip(uint_t samplerate, Parameters::Accessor::Ptr params);
  }  // namespace AYM
}  // namespace Module
