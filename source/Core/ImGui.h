#pragma once

#include <Core.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui.h>
#include <string>

namespace ImGui
{
    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0);
    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0.0f, 0.0f), ImGuiInputTextFlags flags = 0);

    template <typename... Arguments>
    inline void Tooltip(Arguments&&... args)
    {
        if constexpr ((sizeof...(Arguments)) == 1)
        {
            BeginTooltip();
            TextUnformatted(std::forward<Arguments>(args)...);
            EndTooltip();
        }
        else
        {
            SetTooltip(std::forward<Arguments>(args)...);
        }
    }

    inline bool DragUint(const char* label, uint32_t* v, float v_speed = 1.0f, uint32_t v_min = 0, uint32_t v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0, const ImVec2& text_align = ImVec2(0.5f, 0.5f))
    {
        return DragScalar(label, ImGuiDataType_U32, v, v_speed, &v_min, &v_max, format, flags, text_align);
    }

    void BeginBusy(bool isBusy);
    void BeginBusy(const ImVec2& screenPos, const ImVec2& region, bool isBusy);
    void EndBusy(float time, float radius, float thickness, int32_t numSegments, float speed, uint32_t color);
}
// namespace ImGui