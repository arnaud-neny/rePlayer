#pragma once

#include "Proxy.h"

#include <IO/File.h>

#include <RePlayer/Core.h>

#include <atomic>

namespace rePlayer
{
    template <typename StaticType, typename DynamicType>
    inline StaticType* Proxy<StaticType, DynamicType>::Create(DynamicType* buffer)
    {
        auto* owner = Blob::New<StaticType>(sizeof(StaticType));
        owner->id = buffer->id;
        owner->ToProxy(buffer);
        return owner;
    }

    template <typename StaticType, typename DynamicType>
    __declspec(noinline) DynamicType* Proxy<StaticType, DynamicType>::Edit()
    {
        auto* proxy = reinterpret_cast<Info*>(this);
        if (proxy->dataSize != 0)
        {
            auto* edited = new DynamicType();
            reinterpret_cast<StaticType*>(this)->CopyTo(edited);

            ToProxy(edited);
            return edited;
        }
        proxy->reconcileDelay = kReconcileDelay;
        return proxy->buffer;
    }

    template <typename StaticType, typename DynamicType>
    inline DynamicType* Proxy<StaticType, DynamicType>::Dynamic() const
    {
        auto* proxy = reinterpret_cast<const Info*>(this);
        return proxy->buffer;
    }

    template <typename StaticType, typename DynamicType>
    inline void Proxy<StaticType, DynamicType>::ToProxy(DynamicType* buffer)
    {
        auto* proxy = reinterpret_cast<Info*>(this);
        proxy->dataSize = 0;
        proxy->buffer = buffer;
        proxy->reconcileDelay = kReconcileDelay;

        std::atomic_ref(buffer->refCount)++;

        Core::OnNewProxy(buffer->id);
    }

    template <typename StaticType, typename DynamicType>
    inline StaticType* Proxy<StaticType, DynamicType>::Reconcile()
    {
        auto* proxy = reinterpret_cast<Info*>(this);
        if (proxy->dataSize != 0)
            return reinterpret_cast<StaticType*>(this); // already reconciled

        if (proxy->refCount == 1 && proxy->buffer->refCount == 1 && proxy->reconcileDelay-- <= 0)
        {
            auto s = proxy->buffer->Serialize();
            auto buffer = s.Buffer();
            if (buffer.IsNotEmpty())
            {
                auto* blob = Blob::New<StaticType>(buffer.Size());
                memcpy(static_cast<Blob*>(blob) + 1, buffer.Items(), buffer.Size());
                return blob;
            }
            else
                return reinterpret_cast<StaticType*>(this); // keep it always as proxy
        }

        return nullptr; // can't reconcile
    }

    template <typename StaticType, typename DynamicType>
    inline void Proxy<StaticType, DynamicType>::AddRef()
    {
        auto* proxy = reinterpret_cast<StaticType*>(this);
        std::atomic_ref(proxy->refCount)++;
    }

    template <typename StaticType, typename DynamicType>
    inline void Proxy<StaticType, DynamicType>::Release()
    {
        auto* proxy = reinterpret_cast<Info*>(this);
        if (--std::atomic_ref(proxy->refCount) <= 0)
        {
            if (proxy->dataSize == 0)
                Dynamic()->Release();
            reinterpret_cast<StaticType*>(this)->Delete<StaticType>();
        }
    }

    template <typename StaticType, typename DynamicType>
    inline StaticType* Proxy<StaticType, DynamicType>::Load(io::File& file)
    {
        StaticType* blob;
        if (auto size = file.Read<uint16_t>())
        {
            blob = Blob::New<StaticType>(size);
            file.Read(static_cast<Blob*>(blob) + 1, size);
        }
        else
        {
            auto* edited = new DynamicType();
            edited->Load(file);

            blob = Blob::New<StaticType>(sizeof(StaticType));
            blob->id = edited->id;

            auto* proxy = reinterpret_cast<Info*>(blob);
            proxy->dataSize = 0;
            proxy->buffer = edited;
            edited->refCount++;
        }
        return blob;
    }

    template <typename StaticType, typename DynamicType>
    inline void Proxy<StaticType, DynamicType>::Save(io::File& file) const
    {
        auto* proxy = reinterpret_cast<const Info*>(this);
        if (proxy->dataSize == 0)
        {
            auto* edited = Dynamic();

            auto s = edited->Serialize();
            if (s.Save(file) == Status::kFail)
            {
                file.WriteAs<uint16_t>(0);
                edited->Save(file);
            }
        }
        else
        {
            file.WriteAs<uint16_t>(proxy->dataSize);
            file.Write(static_cast<const Blob*>(proxy) + 1, proxy->dataSize);
        }
    }
}
// namespace rePlayer