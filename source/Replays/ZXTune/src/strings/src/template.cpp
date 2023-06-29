/**
 *
 * @file
 *
 * @brief  String template implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// library includes
#include <strings/array.h>
#include <strings/fields.h>
#include <strings/template.h>
// common includes
#include <make_ptr.h>
// std includes
#include <algorithm>
#include <cassert>

namespace Strings
{
  const Char Template::FIELD_START = '[';
  const Char Template::FIELD_END = ']';

  class PreprocessingTemplate : public Template
  {
  public:
    explicit PreprocessingTemplate(const String& templ)
    {
      const std::size_t fieldsAvg = std::count(templ.begin(), templ.end(), FIELD_START);
      FixedStrings.reserve(fieldsAvg);
      Fields.reserve(fieldsAvg);
      Entries.reserve(fieldsAvg * 2);
      ParseTemplate(templ);
    }

    String Instantiate(const FieldsSource& src) const override
    {
      Array resultFields(Fields.size());
      std::transform(Fields.begin(), Fields.end(), resultFields.begin(),
                     [&src](const String& name) { return src.GetFieldValue(name); });
      return SubstFields(resultFields);
    }

  private:
    void ParseTemplate(const String& templ)
    {
      String::size_type textBegin = 0;
      for (;;)
      {
        const auto fieldBegin = templ.find(FIELD_START, textBegin);
        if (String::npos == fieldBegin)
        {
          break;  // no more fields
        }
        const auto fieldEnd = templ.find(FIELD_END, fieldBegin);
        if (String::npos == fieldEnd)
        {
          break;  // invalid syntax
        }
        if (textBegin != fieldBegin)
        {
          // add text to set
          auto text = templ.substr(textBegin, fieldBegin - textBegin);
          const auto idx = FixedStrings.size();
          FixedStrings.emplace_back(std::move(text));
          Entries.emplace_back(idx, false);
        }
        {
          auto field = templ.substr(fieldBegin + 1, fieldEnd - fieldBegin - 1);
          const auto idx = Fields.size();
          Fields.emplace_back(std::move(field));
          Entries.emplace_back(idx, true);
        }
        textBegin = fieldEnd + 1;
      }
      // add rest text
      {
        auto restText = templ.substr(textBegin);
        const auto restIdx = FixedStrings.size();
        FixedStrings.emplace_back(std::move(restText));
        Entries.emplace_back(restIdx, false);
      }
    }

    String SubstFields(const Array& fields) const
    {
      String res;
      for (const auto& entry : Entries)
      {
        res += (entry.second ? fields : FixedStrings)[entry.first];
      }
      return res;
    }

  private:
    Array FixedStrings;
    Array Fields;
    typedef std::pair<std::size_t, bool> PartEntry;  // index => isField
    typedef std::vector<PartEntry> PartEntries;
    PartEntries Entries;
  };

  Template::Ptr Template::Create(const String& templ)
  {
    return MakePtr<PreprocessingTemplate>(templ);
  }

  String Template::Instantiate(const String& templ, const FieldsSource& source)
  {
    return PreprocessingTemplate(templ).Instantiate(source);
  }
}  // namespace Strings
