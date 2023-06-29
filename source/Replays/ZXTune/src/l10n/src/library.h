/**
 *
 * @file
 *
 * @brief    Localization support API
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// common includes
#include <types.h>
// library includes
#include <binary/dump.h>
// std includes
#include <memory>

namespace L10n
{
  /*
    @brief Base translation unit
  */
  class Vocabulary
  {
  public:
    typedef std::shared_ptr<const Vocabulary> Ptr;
    virtual ~Vocabulary() = default;

    //! @brief Retreiving translated or converted text message
    virtual String GetText(const char* text) const = 0;
    /*
      @brief Retreiving tralslated or converted text message according to count
      @note Common algorithm is:
        if (Translation.Available)
          return Translation.GetPluralFor(count)
        else
          return count == 1 ? single : plural;
    */
    virtual String GetText(const char* single, const char* plural, int count) const = 0;

    //! @brief Same functions with context specified
    //! @note Specified design is limited by messages' collecting utilities workflow (context should be specified exactly near the message)
    virtual String GetText(const char* context, const char* text) const = 0;
    virtual String GetText(const char* context, const char* single, const char* plural, int count) const = 0;
  };

  struct Translation
  {
    String Domain;
    String Language;
    String Type;
    Binary::Dump Data;
  };

  class Library
  {
  public:
    virtual ~Library() = default;

    virtual void AddTranslation(const Translation& trans) = 0;

    virtual void SelectTranslation(const String& translation) = 0;

    virtual Vocabulary::Ptr GetVocabulary(const String& domain) const = 0;

    static Library& Instance();
  };
}  // namespace L10n
