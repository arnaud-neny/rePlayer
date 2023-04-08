#pragma once

#include <Core.h>

namespace core
{
    struct Blob
    {
        // Storage is static (read only memory blob) or dynamic (re-allocable data)
        enum Storage : bool
        {
            kIsStatic = false,
            kIsDynamic = true
        };

        template <typename TypeStatic>
        static TypeStatic* New(size_t sizeOfData);

        template <typename TypeStatic>
        void Delete();

        uint16_t dataSize = 0;
        int16_t refCount = 0;
    };
}
// namespace core