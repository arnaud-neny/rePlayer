/**
 *
 * @file
 *
 * @brief  Metadata builder stub implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "formats/chiptune/builder_meta.h"

namespace Formats
{
  namespace Chiptune
  {
    class StubMetaBuilder : public MetaBuilder
    {
    public:
      void SetProgram(StringView /*program*/) override {}
      void SetTitle(StringView /*title*/) override {}
      void SetAuthor(StringView /*author*/) override {}
      void SetStrings(const Strings::Array& /*strings*/) override {}
    };

    MetaBuilder& GetStubMetaBuilder()
    {
      static StubMetaBuilder instance;
      return instance;
    }
  }  // namespace Chiptune
}  // namespace Formats
