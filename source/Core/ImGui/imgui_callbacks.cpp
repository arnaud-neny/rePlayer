#include "imgui_callbacks.h"

#include <ImGui.h>

namespace ImGui
{
    void (*Callbacks::s_createWindow)(ImGuiViewport* vp);
    void (*Callbacks::s_destroyWindow)(ImGuiViewport* vp);

    core::Array<ImGui::Callbacks*> Callbacks::s_callbacks;
    bool Callbacks::s_isRegistered{ false };

    void Callbacks::Register()
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        s_createWindow = platform_io.Platform_CreateWindow;
        s_destroyWindow = platform_io.Platform_DestroyWindow;
        platform_io.Platform_CreateWindow = CreateWindowOverride;
        platform_io.Platform_DestroyWindow = DestroyWindowOverride;
        s_isRegistered = true;
    }

    Callbacks::Callbacks()
    {
        if (!s_isRegistered)
            Register();
        s_callbacks.Add(this);
    }

    Callbacks::~Callbacks()
    {
        s_callbacks.Remove(this);
    }

    void Callbacks::CreateWindowOverride(ImGuiViewport* vp)
    {
        s_createWindow(vp);
        for (auto callback : s_callbacks)
            callback->OnCreateWindow(vp);

    }

    void Callbacks::DestroyWindowOverride(ImGuiViewport* vp)
    {
        for (auto callback : s_callbacks)
            callback->OnDestroyWindow(vp);
        s_destroyWindow(vp);
    }
}
//namespace ImGui