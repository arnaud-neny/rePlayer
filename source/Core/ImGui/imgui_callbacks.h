#pragma once

#include <Containers/Array.h>

struct ImGuiViewport;

namespace ImGui
{
    class Callbacks
    {
    public:
        static void Register();

        Callbacks();
        virtual ~Callbacks();

        virtual void OnCreateWindow(ImGuiViewport* vp) = 0;
        virtual void OnDestroyWindow(ImGuiViewport* vp) = 0;

    private:
        static void CreateWindowOverride(ImGuiViewport* vp);
        static void DestroyWindowOverride(ImGuiViewport* vp);

    private:
        static void (*s_createWindow)(ImGuiViewport* vp);
        static void (*s_destroyWindow)(ImGuiViewport* vp);

        static core::Array<ImGui::Callbacks*> s_callbacks;
        static bool s_isRegistered;
    };
}
//namespace ImGui