/**
 *
 * @file
 *
 * @brief  Vorbis tags parser implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "formats/chiptune/music/tags_vorbis.h"
// common includes
#include <byteorder.h>
// library includes
#include <strings/encoding.h>
#include <strings/trim.h>
// std includes
#include <array>
#include <cctype>

namespace Formats::Chiptune
{
  namespace Vorbis
  {
    StringView ReadString(Binary::DataInputStream& payload)
    {
      const std::size_t size = payload.Read<le_uint32_t>();
      const auto utf8 = safe_ptr_cast<const char*>(payload.ReadData(size).Start());
      return StringView(utf8, size);
    }

    String Decode(StringView str)
    {
      // do not trim before- it may break some encodings
      auto decoded = Strings::ToAutoUtf8(str);
      auto trimmed = Strings::TrimSpaces(decoded);
      return decoded.size() == trimmed.size() ? decoded : trimmed.to_string();
    }

    bool CompareTag(StringView str, StringView tag)
    {
      if (str.size() != tag.size())
      {
        return false;
      }
      for (std::size_t idx = 0, lim = str.size(); idx != lim; ++idx)
      {
        if (std::toupper(str[idx]) != tag[idx])
        {
          return false;
        }
      }
      return true;
    }

    void ParseCommentField(StringView field, MetaBuilder& target)
    {
      const auto eqPos = field.find('=');
      if (eqPos == StringView::npos)
      {
        target.SetStrings({Decode(field)});
        return;
      }
      const auto name = field.substr(0, eqPos);
      const auto value = field.substr(eqPos + 1);
      Strings::Array strings;
      if (CompareTag(name, "TITLE"_sv))
      {
        target.SetTitle(Decode(value));
      }
      else if (CompareTag(name, "ARTIST"_sv) || CompareTag(name, "PERFORMER"_sv))
      {
        target.SetAuthor(Decode(value));
      }
      else if (CompareTag(name, "COPYRIGHT"_sv) || CompareTag(name, "DESCRIPTION"_sv))
      {
        strings.emplace_back(Decode(value));
      }
      // TODO: meta.SetComment
      if (!strings.empty())
      {
        target.SetStrings(strings);
      }
    }

    void ParseComment(Binary::DataInputStream& payload, MetaBuilder& target)
    {
      try
      {
        /*const auto vendor = */ ReadString(payload);
        if (uint_t items = payload.Read<le_uint32_t>())
        {
          while (items--)
          {
            ParseCommentField(ReadString(payload), target);
          }
        }
        Require(1 == payload.ReadByte());
      }
      catch (const std::exception&)
      {}
    }
  }  // namespace Vorbis
}  // namespace Formats::Chiptune
