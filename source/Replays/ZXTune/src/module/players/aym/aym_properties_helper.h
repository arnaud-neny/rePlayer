/**
 *
 * @file
 *
 * @brief  AYM-related Module properties helper interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#include "module/players/properties_helper.h"

#include "string_view.h"

namespace Module::AYM
{
  class PropertiesHelper : public Module::PropertiesHelper
  {
  public:
    explicit PropertiesHelper(Parameters::Modifier& delegate);

    void SetFrequencyTable(StringView freqTable);
    void SetChipType(uint_t type);
    void SetChannelsLayout(uint_t layout);
    void SetChipFrequency(uint64_t freq);
  };
}  // namespace Module::AYM
