/**
 *
 * @file
 *
 * @brief  Parameters accessor interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

#include "parameters/identifier.h"
#include "parameters/types.h"

#include "string_view.h"

#include <memory>
#include <optional>

namespace Parameters
{
  //! @brief Interface to give access to properties and parameters
  //! @invariant If value is not changed, parameter is not affected
  class Accessor
  {
  public:
    //! Pointer type
    using Ptr = std::shared_ptr<const Accessor>;

    virtual ~Accessor() = default;

    //! Content version
    virtual uint_t Version() const = 0;

    //! Accessing integer parameters
    virtual std::optional<IntType> FindInteger(Identifier name) const = 0;
    //! Accessing string parameters
    virtual std::optional<StringType> FindString(Identifier name) const = 0;
    //! Captures snapshot of currently stored data
    virtual Binary::Data::Ptr FindData(Identifier name) const = 0;

    //! Valk along the stored values
    virtual void Process(class Visitor& visitor) const = 0;
  };

  // Helper functions
  inline bool FindValue(const Accessor& src, Identifier name, IntType& res)
  {
    if (auto val = src.FindInteger(name))
    {
      res = *val;
      return true;
    }
    return false;
  }

  template<class T = IntType>
  auto GetInteger(const Accessor& src, Identifier name, IntType defVal = 0)
  {
    return static_cast<T>(src.FindInteger(name).value_or(defVal));
  }

  inline bool FindValue(const Accessor& src, Identifier name, StringType& res)
  {
    if (auto val = src.FindString(name))
    {
      res = std::move(*val);
      return true;
    }
    return false;
  }

  inline auto GetString(const Accessor& src, Identifier name, StringView defVal = ""sv)
  {
    if (auto val = src.FindString(name))
    {
      return std::move(*val);
    }
    return String{defVal};
  }
}  // namespace Parameters
