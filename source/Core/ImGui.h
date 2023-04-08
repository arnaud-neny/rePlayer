#pragma once

#include <Core.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui.h>
#include <string>

namespace ImGui
{
    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0);

    template <typename... Arguments>
    inline void Tooltip(Arguments&&... args)
    {
        if constexpr ((sizeof...(Arguments)) == 1)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(std::forward<Arguments>(args)...);
            ImGui::EndTooltip();
        }
        else
        {
            ImGui::SetTooltip(std::forward<Arguments>(args)...);
        }
    }

    inline bool DragUint(const char* label, uint32_t* v, float v_speed = 1.0f, uint32_t v_min = 0, uint32_t v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0, const ImVec2& text_align = ImVec2(0.5f, 0.5f))
    {
        return ImGui::DragScalar(label, ImGuiDataType_U32, v, v_speed, &v_min, &v_max, format, flags, text_align);
    }
}
//namespace ImGui