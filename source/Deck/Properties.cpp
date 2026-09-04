#include "Properties.h"

// Core
#include <Core/Window.inl.h>
#include <ImGui.h>

// rePlayer
#include <Deck/Player.h>
#include <Graphics/Graphics.h>

namespace rePlayer
{
    Properties::Properties()
        : Window("Properties", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
    {
        RegisterSerializedCustomData(this, [](void* data, const char* line)
        {
            auto* This = reinterpret_cast<Properties*>(data);
            if (strstr(line, "Properties") == line)
            {
                line += sizeof("Properties") - 1;
                MediaType mediaType;
                uint32_t crc;
                if (sscanf_s(line, "Tab%u=%u", &mediaType.value, &crc) == 2)
                {
                    This->m_selectedTabs[mediaType] = crc;
                    return true;
                }
            }
            return false;

        }, [](void* data, ImGuiTextBuffer* buf)
        {
            auto* This = reinterpret_cast<Properties*>(data);
            for (auto it = This->m_selectedTabs.begin(); it != This->m_selectedTabs.end(); ++it)
                buf->appendf("PropertiesTab%u=%u\n", it.Key().value, it.Item());
        });
    }

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
                    ImGui::InputTextMultiline("##", metadata.data(), metadata.size() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        else if (ImGui::BeginTabBar("Properties", ImGuiTabBarFlags_None))
        {
            // Check for saved tab if replay has changed
            // We check the change when a new song is playing or if the replay has changed
            const Replay::Property* selectedProperty = nullptr;
            auto currentId = m_player->GetId();
            auto currentType = m_player->GetMediaType();
            currentType.dummy = 0;
            currentId.subsongId.external = uint16_t(m_player->GetId().databaseId) + (uint16_t(currentType.replay) << 1);
            if (currentId.subsongId.Value() != m_currentSubsongId.Value())
            {
                auto tab = m_selectedTabs[currentType];
                for (auto& property : properties)
                {
                    if (GetCRC(property.label) == tab)
                    {
                        selectedProperty = &property;
                        break;
                    }
                }

                m_currentSubsongId = currentId.subsongId;
            }

            // padding taken from the frame_size of InputTextEx
            auto textPadding = ImGui::GetStyle().FramePadding.y * 2.0f;
            for (auto& property : properties)
            {
                if (ImGui::BeginTabItem(property.label, nullptr, &property == selectedProperty ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
                {
                    m_selectedTabs[currentType] = GetCRC(property.label);
                    if (property.numColumns == 0)
                    {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
                        // read only editable to be able to copy the text in the clipboard
                        ImGui::InputTextMultiline("##", property.data.Items<char>(), property.data.NumItems(), ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
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

    uint32_t Properties::GetCRC(const char* str)
    {
        static constexpr uint32_t kPoly = 0x82f63b78;

        uint32_t crc = 0xffFFffFF;
        while (auto c = *str++)
        {
            crc ^= uint8_t(c);
            for (int i = 0; i < 8; i++)
                crc = crc & 1 ? (crc >> 1) ^ kPoly : crc >> 1;
        }
        return crc;// should be ~crc but don't bother
    }
}
// namespace rePlayer