#pragma once

#include "Blob.h"

namespace core
{
    template <typename TypeStatic>
    inline TypeStatic* Blob::New(size_t sizeOfData)
    {
        assert(sizeOfData < 65536);
        auto buffer = new (Alloc<TypeStatic>(Max(sizeOfData + sizeof(Blob), sizeof(TypeStatic)))) TypeStatic();
        buffer->dataSize = uint16_t(sizeOfData);
        return buffer;
    }

    template <typename TypeStatic>
    void Blob::Delete()
    {
        // we don't call the destructor for a static type, as it's supposed to be a POD
        Free(this);
    }
}
// namespace core