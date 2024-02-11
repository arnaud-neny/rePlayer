#include <Containers/Array.h>
#include <ImGui/imgui_internal.h>

#include "Imgui.h"

namespace ImGui
{
    static auto s_resizeCallback = [](ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            auto& str = *reinterpret_cast<std::string*>(data->UserData);
            str.resize(data->BufTextLen);
            data->Buf = str.data();
        }
        return 0;
    };

    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags)
    {
        return InputText(label, str->data(), str->size() + 1, ImGuiInputTextFlags_CallbackResize | flags, s_resizeCallback, str);
    }

    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags)
    {
        return InputTextMultiline(label, str->data(), str->size() + 1, size, ImGuiInputTextFlags_CallbackResize | flags, s_resizeCallback, str);
    }

    struct BusyData
    {
        ImVec2 startPos;
        ImVec2 region;
        bool isEnabled;
    };
    static core::Array<BusyData> s_busyStack;

    void BeginBusy(bool isBusy)
    {
        s_busyStack.Add({ ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail(), isBusy });
    }

    void BeginBusy(const ImVec2& screenPos, const ImVec2& region, bool isBusy)
    {
        s_busyStack.Add({ screenPos, region, isBusy });
    }

    void EndBusy(float time, float radius, float thickness, int32_t numSegments, float speed, uint32_t color)
    {
        const auto& busyData = s_busyStack.Last();
        if (busyData.isEnabled)
        {
            ImGui::SameLine(0.0f, 0.0f);
            const auto& region = busyData.region;
            auto clampedRadius = core::Min(region.x * 0.5f, region.y * 0.5f, radius);
            thickness *= clampedRadius / radius;

            auto* drawList = GetForegroundDrawList();

            auto scaledTime = time * speed;
            drawList->PathClear();
            auto start = int32_t(fabs(ImSin(scaledTime) * ((231 * numSegments) / 256)));
            const float a_min = IM_PI * 2.0f * (float(start)) / float(numSegments);
            const float a_max = IM_PI * 2.0f * (float((247 * numSegments) / 256)) / float(numSegments);
            const auto centre = busyData.startPos + region * 0.5f;
            for (auto i = 0; i < numSegments; i++)
            {
                const float a = a_min + (float(i) / float(numSegments)) * (a_max - a_min);
                drawList->PathLineTo({ centre.x + ImCos(a + scaledTime * 8) * clampedRadius, centre.y + ImSin(a + scaledTime * 8) * clampedRadius });
            }
            drawList->PathStroke(color, false, thickness);
        }
        s_busyStack.Pop();
    }
}
// namespace ImGui