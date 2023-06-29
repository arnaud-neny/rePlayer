/**
 *
 * @file
 *
 * @brief  PSF program section parser interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// library includes
#include <binary/view.h>
#include <formats/chiptune.h>

namespace Formats
{
  namespace Chiptune
  {
    namespace PlaystationSoundFormat
    {
      const uint_t VERSION_ID = 1;

      class Builder
      {
      public:
        virtual ~Builder() = default;

        virtual void SetRegisters(uint32_t pc, uint32_t gp) = 0;
        virtual void SetStackRegion(uint32_t head, uint32_t size) = 0;
        virtual void SetRegion(String region, uint_t fps) = 0;
        virtual void SetTextSection(uint32_t address, Binary::View content) = 0;
      };

      void ParsePSXExe(Binary::View data, Builder& target);
    }  // namespace PlaystationSoundFormat

    Decoder::Ptr CreatePSFDecoder();
  }  // namespace Chiptune
}  // namespace Formats
