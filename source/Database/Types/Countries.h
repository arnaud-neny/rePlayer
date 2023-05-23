#pragma once

#include <Core.h>
#include <Containers/Array.h>

namespace rePlayer
{
    using namespace core;

    class Countries
    {
    public:
        static int32_t GetIndex(const char* name);
        static uint16_t GetCode(const char* name);
        static const char* GetName(int16_t code);

        static Array<const char*> GetComboList(Array<uint16_t> excluded);

    private:
        static uint16_t GetCode(int32_t index);

    private:
        static const uint16_t ms_countriesId[];
        static const char* const ms_countriesName[];
        static const uint8_t ms_countriesSize[];
    };
}
// namespace rePlayer

#include "Countries.inl"