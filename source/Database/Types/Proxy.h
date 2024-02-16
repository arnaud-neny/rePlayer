#pragma once

#include <Core.h>
#include <Blob/Blob.h>

namespace core::io
{
    class File;
}
// namespace core::io

namespace rePlayer
{
    using namespace core;

    template <typename StaticType, typename DynamicType>
    struct Proxy
    {
        static StaticType* Create(DynamicType* buffer);
        DynamicType* Edit();
        DynamicType* Dynamic() const;

        void ToProxy(DynamicType* buffer);
        StaticType* Reconcile();

        // refcount
        void AddRef();
        void Release();

        // serialize
        static StaticType* Load(io::File& file);
        void Save(io::File& file) const;

        static constexpr int16_t kReconcileDelay = 300;

        struct Info : public Blob
        {
            DynamicType* buffer;
            int16_t reconcileDelay;
        };
    };
}
// namespace rePlayer