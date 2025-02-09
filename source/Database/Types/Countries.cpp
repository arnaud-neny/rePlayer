#include "Countries.h"

#include <string>

namespace rePlayer
{
    #define COUNTRY(a,b,c,d) a,
    const uint16_t Countries::ms_countriesId[] = {
        #include "Countries.inc"
    };
    #undef COUNTRY

    #define COUNTRY(a,b,c,d) b,
    const char* const Countries::ms_countriesName[] = {
        #include "Countries.inc"
    };
    #undef COUNTRY

    #define COUNTRY(a,b,c,d) sizeof(b) - 1,
    const uint8_t Countries::ms_countriesSize[] = {
        #include "Countries.inc"
    };
    #undef COUNTRY

    int32_t Countries::GetIndex(const char* name)
    {
        int16_t countryIndex = -1;
        for (int16_t i = 0; i < NumItemsOf<int16_t>(ms_countriesName); i++)
        {
            if (_strnicmp(ms_countriesName[i], name, ms_countriesSize[i]) == 0)
            {
                countryIndex = i;
                break;
            }
        }

        return countryIndex;
    }

    uint16_t Countries::GetCode(const char* name)
    {
        auto index = GetIndex(name);
        if (index >= 0)
            return ms_countriesId[index];
        return 0;
    }

    const char* Countries::GetName(int16_t code)
    {
        for (int32_t i = 0; i < NumItemsOf(ms_countriesId); i++)
        {
            if (ms_countriesId[i] == code)
                return ms_countriesName[i];
        }

        return "";
    }

    Array<const char*> Countries::GetComboList(Array<uint16_t> excluded)
    {
        Array<const char*> list;

        for (int32_t i = 1; i < NumItemsOf(ms_countriesId); i++)
        {
            if (excluded.Remove(ms_countriesId[i]) < 0)
                list.Add(ms_countriesName[i]);
        }

        struct Compare
        {
            static int func(const void* arg1, const void* arg2)
            {
                /* Compare all of both strings: */
                return _stricmp(*(char**)arg1, *(char**)arg2);
            }
        };
        std::qsort(list.Items(), list.NumItems(), list.ItemSize(), Compare::func);

        return list;
    }
}
// namespace rePlayer