#include <ImGui.h>

#include <Database/Types/Subsong.h>

#include "RatingWidget.h"

namespace rePlayer
{
    bool DrawRating(uint8_t& rating)
    {
        auto oldRating = rating;

        auto pos = ImGui::GetCursorPos();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(RatingStr(oldRating));

        auto min = ImGui::GetItemRectMin();
        auto size = ImGui::GetItemRectSize();
        ImGui::SetCursorPos(pos);
        ImGui::InvisibleButton("Rating", size, ImGuiButtonFlags_MouseButtonLeft);

        if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            rating = uint8_t(Clamp(int(13 * (ImGui::GetMousePos().x - min.x) / size.x), 0, 12));
        return oldRating != rating;
    }
}
// namespace rePlayer