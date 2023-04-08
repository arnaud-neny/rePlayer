#pragma once

#include <Core.h>
#include <Containers/Array.h>

struct ImGuiContext;

namespace core
{
    struct SharedContext
    {
        virtual ~SharedContext() {}
    };

    class SharedContexts
    {
    public:
        static void Register();
        static void Unregister();

        void Init();

    private:
        template <typename Context>
        Context* Get() const;

    private:
        Array<SharedContext*> m_contexts;
        ImGuiContext* m_imguiContext;

    public:
        static SharedContexts* ms_instance;
    };
}
// namespace core