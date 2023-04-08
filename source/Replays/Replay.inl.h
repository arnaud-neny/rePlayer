#pragma once

#include "Replay.h"
#include <ImGui.h>
#include <Core/String.h>

namespace rePlayer
{
    using namespace core;

    template <typename CommandType, auto type>
    inline void Replay::Command<CommandType, type>::SliderOverride(const char* id, auto&& isEnabled, auto&& currentValue, int32_t defaultValue, int32_t min, int32_t max, const char* format, int32_t valueOffset)
    {
        ImGui::PushID(id);
        bool isChecked = isEnabled;
        int32_t sliderValue = isChecked ? int32_t(currentValue) + valueOffset : defaultValue;
        if (ImGui::Checkbox("##Checkbox", &isChecked))
        {
            sliderValue = defaultValue;
            isEnabled = isChecked;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!isChecked);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##Slider", &sliderValue, min, max, format, ImGuiSliderFlags_AlwaysClamp);
        currentValue = isChecked ? sliderValue - valueOffset : 0;
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    template <typename CommandType, auto type>
    template <typename... Items>
    inline void Replay::Command<CommandType, type>::ComboOverride(const char* id, auto&& isEnabled, auto&& currentValue, int32_t defaultValue, Items&&... items)
    {
        ImGui::PushID(id);
        bool isChecked = isEnabled;
        int32_t comboValue = isChecked ? currentValue : defaultValue;
        if (ImGui::Checkbox("##Checkbox", &isChecked))
        {
            comboValue = defaultValue;
            isEnabled = isChecked;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!isChecked);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* const output[] = { std::forward<Items>(items)... };
        ImGui::Combo("##Combo", &comboValue, output, sizeof...(items));
        currentValue = isChecked ? comboValue : 0;
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    template <typename CommandType, auto type>
    inline void Replay::Command<CommandType, type>::Durations(ReplayMetadataContext& context, uint32_t* durations, uint32_t numDurations, const char* format)
    {
        const float buttonSize = ImGui::GetFrameHeight();
        for (uint32_t i = 0; i < numDurations; i++)
        {
            ImGui::PushID(i);
            auto& duration = durations[i];
            char txt[64];
            sprintf(txt, format, i + 1);
            auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4 - buttonSize;
            ImGui::SetNextItemWidth(2.0f * width / 3.0f);
            ImGui::DragUint("##Duration", durations + i, 1000.0f, 0, 0, txt, ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            int32_t milliseconds = duration % 1000;
            int32_t seconds = (duration / 1000) % 60;
            int32_t minutes = duration / 60000;
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3 - buttonSize;
            ImGui::SetNextItemWidth(width / 3.0f);
            ImGui::DragInt("##Minutes", &minutes, 0.1f, 0, 65535, "%d m", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2 - buttonSize;
            ImGui::SetNextItemWidth(width / 2.0f);
            ImGui::DragInt("##Seconds", &seconds, 0.1f, 0, 59, "%d s", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x - buttonSize;
            ImGui::SetNextItemWidth(width);
            ImGui::DragInt("##Milliseconds", &milliseconds, 1.0f, 0, 999, "%d ms", ImGuiSliderFlags_AlwaysClamp);
            duration = uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds);
            ImGui::SameLine();
            if (ImGui::Button("E", ImVec2(buttonSize, 0.0f)))
            {
                context.duration = duration;
                context.songIndex = uint16_t(i);
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && uint32_t(context.songIndex) == i)
            {
                duration = context.duration;
                context.duration = 0;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Open Waveform Viewer");
            ImGui::PopID();
        }
    }
}
// namespace rePlayer