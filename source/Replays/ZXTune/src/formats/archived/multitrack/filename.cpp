/**
 *
 * @file
 *
 * @brief  Multitrack archives support implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#include "formats/archived/multitrack/filename.h"

#include "strings/prefixed_index.h"

#include "string_view.h"

namespace Formats::Archived::MultitrackArchives
{
  const auto FILENAME_PREFIX = "#"sv;

  String CreateFilename(std::size_t index)
  {
    return Strings::PrefixedIndex::Create(FILENAME_PREFIX, index).ToString();
  }

  std::optional<std::size_t> ParseFilename(StringView str)
  {
    const auto parsed = Strings::PrefixedIndex::Parse(FILENAME_PREFIX, str);
    std::optional<std::size_t> result;
    if (parsed.IsValid())
    {
      result = parsed.GetIndex();
    }
    return result;
  }
}  // namespace Formats::Archived::MultitrackArchives
