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
    inline void Replay::Command<CommandType, type>::SliderOverride(const char* id, auto&& isEnabled, auto&& currentValue, float defaultValue, float min, float max, const char* format)
    {
        ImGui::PushID(id);
        bool isChecked = isEnabled;
        float sliderValue = isChecked ? currentValue : defaultValue;
        if (ImGui::Checkbox("##Checkbox", &isChecked))
        {
            sliderValue = defaultValue;
            isEnabled = isChecked;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!isChecked);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Slider", &sliderValue, min, max, format, ImGuiSliderFlags_AlwaysClamp);
        currentValue = isChecked ? sliderValue : 0;
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
    inline bool Replay::Command<CommandType, type>::Loops(ReplayMetadataContext& context, LoopInfo* loops, uint32_t numLoops, uint32_t defaultDuration)
    {
        bool isZero = true;
        const float buttonSize = ImGui::GetFrameHeight();
        char txt[64] = "Loop Start";
        for (uint32_t i = 0; i < numLoops; i++)
        {
            ImGui::PushID(i);

            auto spanWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 8 - buttonSize;

            // checkbox (only if we have a default duration
            float deltaWidth = 0.0f; // so ##StartLoop get smaller
            bool isEnabled = defaultDuration == 0 || loops[i].IsValid();
            LoopInfo loop = isEnabled ? loops[i] : LoopInfo{ 0, defaultDuration };
            if (defaultDuration)
            {
                deltaWidth = ImGui::GetCursorPosX();
                if (ImGui::Checkbox("##Checkbox", &isEnabled))
                    loop = { 0, defaultDuration };
                ImGui::SameLine();
                deltaWidth -= ImGui::GetCursorPosX();
            }
            ImGui::BeginDisabled(!isEnabled);

            // loop start
            if (numLoops > 1)
                sprintf(txt, "Loop Start #%d", i + 1);
            ImGui::SetNextItemWidth(0.33f * spanWidth + deltaWidth);
            ImGui::DragUint("##StartLoop", &loop.start, 1000.0f, 0, 0, txt, ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            int32_t milliseconds = loop.start % 1000;
            int32_t seconds = (loop.start / 1000) % 60;
            int32_t minutes = loop.start / 60000;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(spanWidth * 0.55f / 6.0f);
            ImGui::DragInt("##StartMinutes", &minutes, 0.1f, 0, 65535, "%dm", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(spanWidth * 0.55f / 6.0f);
            ImGui::DragInt("##StartSeconds", &seconds, 0.1f, 0, 59, "%ds", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(spanWidth * 0.55f / 6.0f);
            ImGui::DragInt("##StartMilliseconds", &milliseconds, 1.0f, 0, 999, "%dms", ImGuiSliderFlags_AlwaysClamp);
            loop.start = uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds);
            ImGui::SameLine();

            // loop length
            ImGui::SetNextItemWidth(0.12f * spanWidth);
            ImGui::DragUint("##LengthLoop", &loop.length, 1000.0f, 0, 0, "Length", ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            milliseconds = loop.length % 1000;
            seconds = (loop.length / 1000) % 60;
            minutes = loop.length / 60000;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(spanWidth * 0.55f / 6.0f);
            ImGui::DragInt("##LengthMinutes", &minutes, 0.1f, 0, 65535, "%dm", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(spanWidth * 0.55f / 6.0f);
            ImGui::DragInt("##LengthSeconds", &seconds, 0.1f, 0, 59, "%ds", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(spanWidth * 0.55f / 6.0f);
            ImGui::DragInt("##LengthMilliseconds", &milliseconds, 1.0f, 0, 999, "%dms", ImGuiSliderFlags_AlwaysClamp);
            loop.length = uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds);
            ImGui::SameLine();

            if (ImGui::Button("E", ImVec2(-FLT_MIN, 0.0f)))
            {
                context.loop = loop;
                context.subsongIndex = uint16_t(i);
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.loop.IsValid() && uint32_t(context.subsongIndex) == i)
            {
                loop = context.loop;
                context.loop = {};
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Open Waveform Viewer");

            loops[i] = isEnabled ? loop : LoopInfo{};
            isZero &= !loop.IsValid();

            ImGui::EndDisabled();
            ImGui::PopID();
        }
        return isZero;
    }
}
// namespace rePlayer