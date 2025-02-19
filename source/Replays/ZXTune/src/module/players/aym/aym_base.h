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

#include "module/players/aym/aym_chiptune.h"

#include "module/holder.h"

namespace Module::AYM
{
  class Holder : public Module::Holder
  {
  public:
    using Ptr = std::shared_ptr<const Holder>;

    using Module::Holder::CreateRenderer;
    virtual AYM::Chiptune::Ptr GetChiptune() const = 0;

    // TODO: move to another place
    virtual void Dump(Devices::AYM::Device& dev) const = 0;
  };

  Holder::Ptr CreateHolder(Chiptune::Ptr chiptune);

  Devices::AYM::Chip::Ptr CreateChip(uint_t samplerate, Parameters::Accessor::Ptr params);
}  // namespace Module::AYM
