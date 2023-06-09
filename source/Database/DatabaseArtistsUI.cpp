#include "DatabaseArtistsUI.h"

#include <Core/Window.inl.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>
#include <IO/File.h>

#include <Database/Types/Countries.h>
#include <Database/Database.h>
#include <Database/DatabaseSongsUI.h>
#include <RePlayer/Core.h>

#include <algorithm>

namespace rePlayer
{
    DatabaseArtistsUI::DatabaseArtistsUI(DatabaseID databaseId, Window& owner)
        : m_db(Core::GetDatabase(databaseId))
        , m_owner(owner)
        , m_databaseId(databaseId)
        , m_artistFilter(new ImGuiTextFilter())
        , m_artistMerger{ new ImGuiTextFilter() }
    {
        m_db.Register(this);
        owner.RegisterSerializedData(m_artistFilter->InputBuf, "ArtistsFilter", &OnArtistFilterLoaded, uintptr_t(m_artistFilter));
    }

    DatabaseArtistsUI::~DatabaseArtistsUI()
    {
        delete m_artistFilter;
        delete m_artistMerger.filter;
    }

    void DatabaseArtistsUI::OnDisplay()
    {
        bool isDirty = m_dbRevision != m_db.ArtistsRevision();
        m_dbRevision = m_db.ArtistsRevision();
        Artist* selectedArtist = m_db.IsValid(m_selectedArtistCopy.id) ? m_db[m_selectedArtistCopy.id] : nullptr;
        if (selectedArtist != nullptr)
        {
            m_selectedArtistCopy.numSongs = selectedArtist->NumSongs();
            m_selectedArtistCopy.sources = selectedArtist->Sources();
        }
        else
            m_selectedArtistCopy = {};
        if (m_selectedArtistCopy.handles.IsEmpty())
            m_selectedArtistCopy.handles.Push();

        bool isFiltering = isDirty && m_artistFilter->IsActive();
        if (m_artistFilter->Draw("##filter", -1) || isFiltering)
        {
            m_artists.Clear();
            for (Artist* artist : m_db.Artists())
            {
                if (m_artistFilter->PassFilter(artist->GetHandle()))
                    m_artists.Add(artist->GetId());
            }
            isDirty = true;
        }
        else if (isDirty)
        {
            m_artists.Clear();
            for (Artist* artist : m_db.Artists())
                m_artists.Add(artist->GetId());
        }

        if (ImGui::BeginTable("Artists", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 128.0f, 0);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);

            ImGui::TableNextColumn();

            if (ImGui::BeginTable("artists", 1, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg
                | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("Artist", 0, 0.0f, 0);
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                auto* sortsSpecs = ImGui::TableGetSortSpecs();
                if (sortsSpecs && (sortsSpecs->SpecsDirty || isDirty))
                {
                    if (m_artists.NumItems() > 1)
                    {
                        std::sort(m_artists.begin(), m_artists.end(), [&](ArtistID l, ArtistID r)
                        {
                            auto* lArtist = m_db[l];
                            auto* rArtist = m_db[r];
                            for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                            {
                                auto& sortSpec = sortsSpecs->Specs[i];
                                int32_t delta = _stricmp(lArtist->GetHandle(), rArtist->GetHandle());
                                if (delta)
                                    return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                            }
                            return l < r;
                        });
                    }
                    sortsSpecs->SpecsDirty = false;
                }

                ImGuiListClipper clipper;
                clipper.Begin(m_artists.NumItems<int32_t>());
                while (clipper.Step())
                {
                    for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                    {
                        ArtistID artistId = m_artists[rowIdx];
                        auto* artist = m_db[artistId];

                        ImGui::TableNextColumn();
                        ImGui::PushID(static_cast<int>(artistId));
                        bool isSelected = selectedArtist != nullptr && artistId == selectedArtist->GetId();
                        if (isSelected = ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastFirstRowHeight)))//TBD: using imgui_internal for row height
                        {
                            selectedArtist = artist;
                            if (m_selectedArtistCopy.id != artistId)
                            {
                                artist->CopyTo(&m_selectedArtistCopy);
                                m_selectedCountry = -1;
                            }
                        }
                        ImGui::SameLine(0.0f, 0.0f);//no spacing
                        ImGui::TextUnformatted(artist->GetHandle());
                        ImGui::PopID();
                    }
                }

                ImGui::EndTable();
            }

            ImGui::TableNextColumn();

            if (ImGui::BeginTable("artist", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);

                IdUI(selectedArtist);
                HandleUI(selectedArtist);
                AliasUI(selectedArtist);
                NameUI(selectedArtist);
                GroupUI(selectedArtist);
                CountryUI(selectedArtist);
                SaveChangesUI(selectedArtist);
                SourcesUI(selectedArtist);
                MergeUI(selectedArtist);
                SongUI(selectedArtist);

                ImGui::EndTable();
            }
            ImGui::EndTable();

            if (m_artistMerger.isActive && m_artistMerger.masterArtist != nullptr)
                ImGui::OpenPopup("Merge Artist");
            ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
            if (ImGui::BeginPopupModal("Merge Artist", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Discard: %s / %04X\nTarget: %s / %04X", m_artistMerger.mergedArtist->GetHandle(), static_cast<uint32_t>(m_artistMerger.mergedArtist->GetId())
                    , m_artistMerger.masterArtist->GetHandle(), static_cast<uint32_t>(m_artistMerger.masterArtist->GetId()));
                if (ImGui::Button("Proceed", ImVec2(120, 0)))
                {
                    auto* songs = m_db.GetSongsUI();
                    auto* mergedArtist = m_artistMerger.mergedArtist;
                    auto* masterArtist = m_artistMerger.masterArtist->Edit();
                    auto mergedArtistId = mergedArtist->GetId();
                    auto masterArtistId = masterArtist->id;

                    for (auto* song : m_db.Songs())
                    {
                        std::string oldFilename;
                        uint16_t dupArtistPos = song->NumArtistIds();
                        for (uint16_t i = 0; i < song->NumArtistIds(); i++)
                        {
                            //replace the merge id by the master id or remove it if it is already in the list
                            if (song->GetArtistId(i) == masterArtistId)
                            {
                                if (dupArtistPos != song->NumArtistIds())
                                {
                                    masterArtist->numSongs--;
                                    auto* songSheet = song->Edit();
                                    songSheet->artistIds.RemoveAt(i);
                                    break;
                                }
                                else
                                    dupArtistPos = i;
                            }
                            else if (song->GetArtistId(i) == mergedArtistId)
                            {
                                if (song->GetFileSize() != 0 && oldFilename.empty() && m_databaseId == DatabaseID::kLibrary)
                                    oldFilename = songs->GetFullpath(song);
                                if (dupArtistPos != song->NumArtistIds())
                                {
                                    song->Edit()->artistIds.RemoveAt(i);
                                    break;
                                }
                                else
                                {
                                    masterArtist->numSongs++;
                                    dupArtistPos = i;
                                    auto* songSheet = song->Edit();
                                    songSheet->artistIds[i] = masterArtistId;
                                }
                            }
                        }
                        if (!oldFilename.empty())
                        {
                            auto newFilename = songs->GetFullpath(song);
                            io::File::Move(oldFilename.c_str(), newFilename.c_str());
                        }
                    }

                    if (masterArtist->realName.IsEmpty())
                        masterArtist->realName = mergedArtist->GetRealName();
                    for (size_t j = 0, mergedSize = mergedArtist->NumHandles(), masterSize = masterArtist->handles.NumItems(); j < mergedSize; j++)
                    {
                        for (size_t i = 0;;)
                        {
                            if (strcmp(mergedArtist->GetHandle(j), masterArtist->handles[i].Items()) == 0)
                                break;
                            if (++i == masterSize)
                            {
                                masterArtist->handles.Add(mergedArtist->GetHandle(j));
                                break;
                            }
                        }
                    }
                    for (size_t j = 0, mergedSize = mergedArtist->NumCountries(), masterSize = masterArtist->countries.NumItems(); j < mergedSize; j++)
                    {
                        for (size_t i = 0;;)
                        {
                            if (mergedArtist->GetCountry(j) == masterArtist->countries[i])
                                break;
                            if (++i == masterSize)
                            {
                                masterArtist->countries.Add(mergedArtist->GetCountry(j));
                                break;
                            }
                        }
                    }
                    for (size_t j = 0, mergedSize = mergedArtist->NumGroups(), masterSize = masterArtist->groups.NumItems(); j < mergedSize; j++)
                    {
                        for (size_t i = 0;;)
                        {
                            if (strcmp(mergedArtist->GetGroup(j), masterArtist->groups[i].Items()) == 0)
                                break;
                            if (++i == masterSize)
                            {
                                masterArtist->groups.Add(mergedArtist->GetGroup(j));
                                break;
                            }
                        }
                    }
                    if (m_databaseId == DatabaseID::kLibrary)
                    {
                        for (auto& source : mergedArtist->Sources())
                            masterArtist->sources.Add(source);
                    }

                    m_db.RemoveArtist(mergedArtist->GetId());

                    m_db.Raise(Database::Flag::kSaveArtists | Database::Flag::kSaveSongs);

                    m_selectedArtistCopy = {};
                    m_selectedCountry = -1;
                    m_artistMerger.masterArtist = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    m_artistMerger.masterArtist = nullptr;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void DatabaseArtistsUI::SourcesUI(Artist* selectedArtist)
    {
        (void)selectedArtist;
    }

    void DatabaseArtistsUI::OnSavedChanges(Artist* selectedArtist)
    {
        auto selectedArtistEdit = selectedArtist->Edit();
        selectedArtistEdit->realName = m_selectedArtistCopy.realName;
        selectedArtistEdit->handles = m_selectedArtistCopy.handles;
        selectedArtistEdit->countries = m_selectedArtistCopy.countries;
        selectedArtistEdit->groups = m_selectedArtistCopy.groups;
        m_db.Raise(Database::Flag::kSaveArtists);
    }

    void DatabaseArtistsUI::OnArtistFilterLoaded(uintptr_t userData, void*, const char*)
    {
        auto* artistFilter = reinterpret_cast<ImGuiTextFilter*>(userData);
        artistFilter->Build();
    }

    void DatabaseArtistsUI::IdUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("ID:");
        ImGui::TableNextColumn();
        if (selectedArtist)
            ImGui::Text("%04X", static_cast<uint32_t>(selectedArtist->GetId()));
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::HandleUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Handle:");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##Handle", &m_selectedArtistCopy.handles[0].String(), selectedArtist ? 0 : ImGuiInputTextFlags_ReadOnly);
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::AliasUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Alias:");
        ImGui::TableNextColumn();
        if (selectedArtist)
        {
            const float button_size = ImGui::GetFrameHeight();
            auto& style = ImGui::GetStyle();

            uint16_t handleToSwap = 0;
            uint16_t handleToRemove = 0;
            for (uint16_t i = 1; i < m_selectedArtistCopy.handles.NumItems(); i++)
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
                ImGui::InputText("##Handle", &m_selectedArtistCopy.handles[i].String());
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                if (ImGui::Button("M", ImVec2(button_size, button_size)))
                    handleToSwap = i;
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Set as Main Handle");
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                if (ImGui::Button("X", ImVec2(button_size, button_size)))
                    handleToRemove = i;
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Remove alias");
                ImGui::PopID();
            }
            if (handleToSwap)
                std::swap(m_selectedArtistCopy.handles[0], m_selectedArtistCopy.handles[handleToSwap]);
            if (handleToRemove)
                m_selectedArtistCopy.handles.RemoveAt(handleToRemove);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Max(1.0f, ImGui::CalcItemWidth() - button_size));
            if (ImGui::Button("+", ImVec2(button_size, button_size)))
                m_selectedArtistCopy.handles.Add("");
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Add another alias");
        }
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::NameUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Name:");
        ImGui::TableNextColumn();
        if (selectedArtist)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##Name", &m_selectedArtistCopy.realName.String());
        }
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::GroupUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Group:");
        ImGui::TableNextColumn();
        if (selectedArtist)
        {
            ImGui::PushID("EditGroups");
            const float button_size = ImGui::GetFrameHeight();
            auto& style = ImGui::GetStyle();

            uint16_t groupToRemove = m_selectedArtistCopy.groups.NumItems<uint16_t>();
            for (uint16_t i = 0, e = groupToRemove; i < e; i++)
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
                ImGui::InputText("##Group", &m_selectedArtistCopy.groups[i].String());
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                if (ImGui::Button("X", ImVec2(button_size, button_size)))
                    groupToRemove = i;
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Remove group");
                ImGui::PopID();
            }
            if (groupToRemove != m_selectedArtistCopy.groups.NumItems())
                m_selectedArtistCopy.groups.RemoveAt(groupToRemove);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Max(1.0f, ImGui::CalcItemWidth() - button_size));
            if (ImGui::Button("+", ImVec2(button_size, button_size)))
                m_selectedArtistCopy.groups.Add("");
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Add another group");
            ImGui::PopID();
        }
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::CountryUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Country:");
        ImGui::TableNextColumn();
        if (selectedArtist)
        {
            ImGui::PushID("EditCountries");
            const float button_size = ImGui::GetFrameHeight();
            auto& style = ImGui::GetStyle();

            uint16_t countryToRemove = m_selectedArtistCopy.countries.NumItems<uint16_t>();
            for (uint16_t i = 0, e = countryToRemove; i < e; i++)
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(m_selectedArtistCopy.countries[i]);
                ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
                ImGui::InputText("##country", const_cast<char*>(Countries::GetName(m_selectedArtistCopy.countries[i])), strlen(Countries::GetName(m_selectedArtistCopy.countries[i])) + 1, ImGuiInputTextFlags_ReadOnly);
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                if (ImGui::Button("X", ImVec2(button_size, button_size)))
                    countryToRemove = i;
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Remove country");
                ImGui::PopID();
            }
            if (countryToRemove != m_selectedArtistCopy.countries.NumItems())
                m_selectedArtistCopy.countries.RemoveAt(countryToRemove);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x)));
            auto countries = Countries::GetComboList(m_selectedArtistCopy.countries.Container());
            ImGui::Combo("##NewCountry", &m_selectedCountry, countries.Items(), countries.NumItems<int32_t>());
            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(m_selectedCountry == -1);
            if (ImGui::Button("+", ImVec2(button_size, button_size)))
            {
                m_selectedArtistCopy.countries.Add(Countries::GetCode(countries[m_selectedCountry]));
                m_selectedCountry = -1;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Add another country");
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::SaveChangesUI(Artist* selectedArtist)
    {
        if (selectedArtist)
        {
            auto isUpdated = !selectedArtist->AreSameHandles(&m_selectedArtistCopy);
            isUpdated |= selectedArtist->Countries() != m_selectedArtistCopy.countries;
            isUpdated |= !selectedArtist->AreSameGroups(&m_selectedArtistCopy);
            isUpdated |= selectedArtist->GetRealName() != m_selectedArtistCopy.realName;

            ImGui::BeginDisabled(!isUpdated);
            if (ImGui::Button("Save Changes", ImVec2(-FLT_MIN, 0)))
                OnSavedChanges(selectedArtist);
            ImGui::EndDisabled();
        }
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::MergeUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::Spacing();
        if (selectedArtist)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 1.0f));
            ImGui::BeginDisabled(m_db.NumArtists() <= 1);
            if (ImGui::Button("Merge With Another Artist", ImVec2(-FLT_MIN, 0)))
                ImGui::OpenPopup("MergeArtist");
            ImGui::PopStyleColor(3);
            if (ImGui::BeginPopupContextItem("MergeArtist"))
            {
                m_artistMerger.mergedArtist = selectedArtist;
                if (SelectMasterArtist(selectedArtist->GetId()))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::EndDisabled();
        }
        ImGui::Spacing();
    }

    void DatabaseArtistsUI::SongUI(Artist* selectedArtist)
    {
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextUnformatted("Songs:");
        ImGui::Spacing();
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::Spacing();
        if (selectedArtist)
        {
            ImGui::Text("%u", selectedArtist->NumSongs());
            //TODO: cache the song list
            std::string songTitle;
            for (auto* song : m_db.Songs())
            {
                for (auto artistId : song->ArtistIds())
                {
                    if (artistId == selectedArtist->GetId())
                    {
                        auto songId = song->GetId();
                        songTitle = m_db.GetArtists(songId);
                        songTitle += " - ";
                        songTitle += song->GetName();
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::PushID(static_cast<uint32_t>(songId));
                        ImGui::InputText("##Title", &songTitle, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopID();
                        break;
                    }
                }
            }
        }
        ImGui::Spacing();
    }

    bool DatabaseArtistsUI::SelectMasterArtist(ArtistID artistId)
    {
        if (m_artistMerger.filter->Draw("##filter", -1) || m_artistMerger.filter->IsActive() || !m_artistMerger.isActive)
        {
            m_artistMerger.artists.Clear();

            for (auto artist : m_db.Artists())
            {
                if (artist->GetId() != artistId && m_artistMerger.filter->PassFilter(artist->GetHandle()))
                    m_artistMerger.artists.Add(artist->GetId());
            }
        }
        m_artistMerger.isActive = true;

        ImGuiListClipper clipper;
        clipper.Begin(m_artistMerger.artists.NumItems<int32_t>());
        while (clipper.Step())
        {
            for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
            {
                auto* artist = m_db[m_artistMerger.artists[rowIdx]];
                std::string label = artist->GetHandle();
                if (artist->NumSources() != 0)
                {
                    label += " / ";
                    label += artist->GetSource(0).GetName();
                }
                if (ImGui::MenuItem(label.c_str()))
                    m_artistMerger.masterArtist = artist;
            }
        }

        return m_artistMerger.masterArtist != nullptr;
    }
}
// namespace rePlayer