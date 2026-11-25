// core
#include <ImGui.h>

// rePlayer
#include <Database/Database.h>
#include <Playlist/Playlist.h>
#include <Playlist/Playlist.inl.h>
#include <Replayer/Core.h>

#include "Deck.h"

// system :(
#include <windows.h>

namespace rePlayer
{
    void Deck::PatchRates()
    {                       
        if (!m_areRatingsDirty)
            return;

        static constexpr uint32_t kVersionRating = Version(0, 22, 19);
        if ((Core::GetDatabase(DatabaseID::kLibrary).GetSongsVersion() == 0 || Core::GetDatabase(DatabaseID::kLibrary).GetSongsVersion() > kVersionRating)
            && (Core::GetDatabase(DatabaseID::kPlaylist).GetSongsVersion() == 0 || Core::GetDatabase(DatabaseID::kPlaylist).GetSongsVersion() > kVersionRating))
        {
            m_areRatingsDirty = false;
            return;
        }

        ImGui::OpenPopup("Patch Ratings");

        // Compute center of the primary OS monitor using Win32
        POINT ptZero = {};
        MONITORINFO mi = {
            .cbSize = sizeof(mi)
        };
        GetMonitorInfo(MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY), &mi);

        // Position and render the popup
        ImGui::SetNextWindowPos(ImVec2((mi.rcMonitor.left + mi.rcMonitor.right) * 0.5f, (mi.rcMonitor.top + mi.rcMonitor.bottom) * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Patch Ratings", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            static int ranges[12][2] = { { 0, 8 }, { 9, 17 }, { 18, 26 }, { 27, 35 }, { 36, 44 }
                , { 45, 53 }, { 54, 62 }, { 63, 71 }, { 72, 80 }, { 81, 89 }, { 90, 98 }
                , { 98, 100 } };

            for (int i = 0; i < 12; ++i)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(RatingStr(i + 1));
                ImGui::SameLine();

                char label[] = "##00Slider";
                label[2] += char(i / 10);
                label[3] += char(i % 10);

                if (i == 0)
                {
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragInt("##0", &ranges[0][0], 1.0f, 0, 0, "%d", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::DragInt(label, &ranges[0][1], 1.0f, 0, ranges[1][1] - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
                        ranges[1][0] = ranges[0][1] + 1;
                }
                else if (i == 11)
                {
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::DragInt(label, &ranges[11][0], 1.0f, ranges[10][0] + 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp))
                        ranges[10][1] = ranges[11][0] - 1;
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragInt("##1", &ranges[11][1], 1.0f, 100, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
                }
                else
                {
                    ImGui::SetNextItemWidth(200 + ImGui::GetStyle().ItemSpacing.x);
                    if (ImGui::DragIntRange2(label, &ranges[i][0], &ranges[i][1], 1.0f, ranges[i - 1][0] + 1, ranges[i + 1][1] - 1, "%d", "%d", ImGuiSliderFlags_AlwaysClamp))
                    {
                        ranges[i + 1][0] = ranges[i][1] + 1;
                        ranges[i - 1][1] = ranges[i][0] - 1;
                    }
                }
            }

            if (ImGui::Button("Process", ImVec2(-FLT_MIN, 0.0f)))
            {
                auto updateDatabase = [&](Database& db)
                {
                    db.Raise(Database::Flag::kSaveSongs);
                    if (db.m_oldRatings.IsNotEmpty())
                    {
                        for (auto subsongId : db.m_oldRatings)
                        {
                            auto* song = db[subsongId]->Edit();

                            for (int i = 0; i < 12; ++i)
                            {
                                if (subsongId.external >= ranges[i][0] && subsongId.external <= ranges[i][1])
                                {
                                    song->subsongs[subsongId.index].rating = i + 1;
                                    break;
                                }
                            }
                        }
                        db.m_oldRatings.Reset();
                    }
                };

                for (int i = 0; i < int(DatabaseID::kCount); ++i)
                    updateDatabase(Core::GetDatabase(DatabaseID(i)));

                Core::GetPlaylist().ForEachPlaylist(updateDatabase);

                ImGui::CloseCurrentPopup();
                m_areRatingsDirty = false;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Since version 0.22.20, ratings are set using stars instead of a percentage\nUpdate here the conversion ranges from percentage to stars/favorite and databases will be converted\nYou will never see this window after that.");

            ImGui::EndPopup();
        }
    }

}
// namespace rePlayer