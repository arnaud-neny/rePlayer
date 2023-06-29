/**
 *
 * @file
 *
 * @brief  2SF parser interface
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
    namespace NintendoDSSoundFormat
    {
      const uint_t VERSION_ID = 0x24;

      class Builder
      {
      public:
        virtual ~Builder() = default;

        virtual void SetChunk(uint32_t offset, Binary::View content) = 0;
      };

      void ParseRom(Binary::View data, Builder& target);
      void ParseState(Binary::View data, Builder& target);
    }  // namespace NintendoDSSoundFormat

    Decoder::Ptr Create2SFDecoder();
  }  // namespace Chiptune
}  // namespace Formats
