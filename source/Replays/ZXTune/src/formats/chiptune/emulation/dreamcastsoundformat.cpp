/**
 *
 * @file
 *
 * @brief  DSF parser implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "formats/chiptune/emulation/dreamcastsoundformat.h"
// common includes
#include <make_ptr.h>
// library includes
#include <binary/format_factories.h>

namespace Formats::Chiptune
{
  namespace DreamcastSoundFormat
  {
    const Char DESCRIPTION[] = "Dreamcast Sound Format";

    const auto FORMAT =
        "'P'S'F"
        "12"
        ""_sv;

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::CreateMatchOnlyFormat(FORMAT))
      {}

      String GetDescription() const override
      {
        return DESCRIPTION;
      }

      Binary::Format::Ptr GetFormat() const override
      {
        return Format;
      }

      bool Check(Binary::View rawData) const override
      {
        return Format->Match(rawData);
      }

      Formats::Chiptune::Container::Ptr Decode(const Binary::Container& /*rawData*/) const override
      {
        return {};  // TODO
      }

    private:
      const Binary::Format::Ptr Format;
    };
  }  // namespace DreamcastSoundFormat

  Decoder::Ptr CreateDSFDecoder()
  {
    return MakePtr<DreamcastSoundFormat::Decoder>();
  }
}  // namespace Formats::Chiptune
