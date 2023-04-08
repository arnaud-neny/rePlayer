#include "SharedContext.h"

#include <Core/Log.h>
#include <Imgui/imgui.h>

namespace core
{
    SharedContexts* SharedContexts::ms_instance = nullptr;

    void SharedContexts::Register()
    {
        ms_instance = new SharedContexts();

        ms_instance->m_contexts.Add(Log::ms_instance = new Log());
        ms_instance->m_imguiContext = ImGui::GetCurrentContext();
    }

    void SharedContexts::Unregister()
    {
        for (auto context : ms_instance->m_contexts)
            delete context;
        delete ms_instance;
    }

    void SharedContexts::Init()
    {
        Log::ms_instance = Get<Log>();

        ImGui::SetAllocatorFunctions([](size_t size, void*) { return Alloc(size, 0); }, [](void* ptr, void*) { Free(ptr); });
        ImGui::SetCurrentContext(m_imguiContext);
    }

    template <typename Context>
    Context* SharedContexts::Get() const
    {
        for (auto context : m_contexts)
        {
            if (auto castContext = dynamic_cast<Context*>(context))
                return castContext;
        }
        return nullptr;
    }
}
// namespace core