#include "Properties.h"

#include <Core/Window.inl.h>
#include <ImGui.h>

#include <Deck/Player.h>
#include <Graphics/Graphics.h>

namespace rePlayer
{
    Properties::Properties()
        : Window("Properties", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
    {}

    Properties::~Properties()
    {}

    void Properties::Update(Player* player)
    {
        m_player = player;
    }

    std::string Properties::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 600.0f), ImGuiCond_FirstUseEver);

        return "Properties";
    }

    void Properties::OnDisplay()
    {
        if (m_player == nullptr)
            return;

        auto& properties = m_player->GetProperties();
        if (properties.IsEmpty())
        {
            // fallback to metadata if any
            auto metadata = m_player->GetMetadata();
            if (metadata.empty())
                return;
            if (ImGui::BeginTabBar("Properties", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("Metadata"))
                {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
                    ImGui::InputTextMultiline("##", metadata.data(), metadata.size() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
                    ImGui::PopStyleColor();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        if (ImGui::BeginTabBar("Properties", ImGuiTabBarFlags_None))
        {
            // padding taken from the frame_size of InputTextEx
            auto textPadding = ImGui::GetStyle().FramePadding.y * 2.0f;
            for (auto& property : properties)
            {
                if (ImGui::BeginTabItem(property.label))
                {
                    if (property.numColumns == 0)
                    {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
                        // read only editable to be able to copy the text in the clipboard
                        ImGui::InputTextMultiline("##", property.data.Items<char>(), property.data.NumItems(), ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        auto* entry = property.First();
                        if (ImGui::BeginTable(entry->txt, property.numColumns, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
                        {
                            for (uint32_t i = 0; i < property.numEntries; ++i)
                            {
                                ImGui::TableNextColumn();
                                if (entry->isEditable)
                                {
                                    ImGui::PushID(i);
                                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
                                    // read only editable to be able to copy the text in the clipboard
                                    ImGui::InputTextMultiline("##", pCast<char>(entry->txt), entry->Size(), ImVec2(-FLT_MIN, ImGui::CalcTextSize(entry->txt, entry->txt + entry->Size() - 1).y + textPadding), ImGuiInputTextFlags_ReadOnly);
                                    ImGui::PopStyleColor();
                                    ImGui::PopID();
                                }
                                else
                                    ImGui::TextUnformatted(entry->txt, entry->txt + entry->Size());
                                entry = entry->Next();
                            }


                            ImGui::EndTable();
                        }
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
}
// namespace rePlayer