/**
 *
 * @file
 *
 * @brief  Shared library interface
 *
 * @author vitamin.caig@gmail.com
 *
 **/

#pragma once

// std includes
#include <string>
#include <vector>

namespace Platform
{
  class SharedLibrary
  {
  public:
    typedef std::shared_ptr<const SharedLibrary> Ptr;
    virtual ~SharedLibrary() = default;

    virtual void* GetSymbol(const String& name) const = 0;

    //! @param name Library name without platform-dependent prefixes and extension
    // E.g. Load("SDL") will try to load "libSDL.so" for Linux and and "SDL.dll" for Windows
    // If platform-dependent full filename is specified, no substitution is made
    static Ptr Load(const String& name);

    class Name
    {
    public:
      virtual ~Name() = default;

      virtual String Base() const = 0;
      virtual std::vector<String> PosixAlternatives() const = 0;
      virtual std::vector<String> WindowsAlternatives() const = 0;
    };

    static Ptr Load(const Name& name);
  };
}  // namespace Platform
