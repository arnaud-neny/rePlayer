// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>

// rePlayer
#include <Database/Types/Countries.h>
#include <Database/Database.h>

#include "DatabaseArtistsUI.h"

// stl
#include <algorithm>

namespace rePlayer
{
    DatabaseArtistsUI::DatabaseArtistsUI(DatabaseID databaseId, Window& owner)
        : DatabaseSongsUI(databaseId, owner, false, (1 << kSize) + (1 << kYear) + (1 << kCRC) + (1 << kState) + (1 << kRating) + (1 << kDatabaseDate) + (1 << kSource) + (1 << kReplay),"Artists")
        , m_artistFilter(new ImGuiTextFilter())
        , m_artistPicker{ new ImGuiTextFilter() }
    {
        owner.RegisterSerializedData(m_artistFilter->InputBuf, "ArtistsFilter", &OnArtistFilterLoaded, uintptr_t(m_artistFilter));
        owner.RegisterSerializedData(m_selectedArtistCopy.id, "ArtistsSelection", &OnArtistSelectionLoaded, uintptr_t(this));
    }

    DatabaseArtistsUI::~DatabaseArtistsUI()
    {
        delete m_artistFilter;
        delete m_artistPicker.filter;
    }

    void DatabaseArtistsUI::OnDisplay()
    {
        bool isDirty = m_dbArtistsRevision != m_db.ArtistsRevision();
        m_dbArtistsRevision = m_db.ArtistsRevision();
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

        if (ImGui::BeginTable("Artists", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit, ImVec2(0.0f, -FLT_MIN)))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 128.0f, 0);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);

            ImGui::TableNextColumn();

            // display the artists
            if (ImGui::BeginTable("artists", 1, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg
                | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("Artist", 0, 0.0f, 0);
                ImGui::TableSetupScrollFreeze(0, 2); // Make rows always visible
                ImGui::TableHeadersRow();

                // display and process the artist search bar
                ImGui::TableNextColumn();
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

                // sort everything
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

                // display the artists list
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
                        if (isSelected = ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastTopHeadersRowHeight)))//TBD: using imgui_internal for row height
                        {
                            selectedArtist = artist;
                            if (m_selectedArtistCopy.id != artistId)
                            {
                                m_dbSongsRevision = m_db.SongsRevision() + 1;
                                selectedArtist->CopyTo(&m_selectedArtistCopy);
                            }
                        }
                        ImGui::SameLine(0.0f, 0.0f);//no spacing
                        ImGui::TextUnformatted(artist->GetHandle());
                        ImGui::PopID();
                    }
                }

                // build the tracking position and set it if the scrolling is enabled for the table
                if (m_isTrackingArtist && m_trackedSubsongId.IsValid())
                {
                    auto* song = m_db[m_trackedSubsongId];
                    ArtistID selectedArtistId = ArtistID::Invalid;
                    for (uint32_t i = 0, e = song->NumArtistIds(); i < e; ++i)
                    {
                        if (song->GetArtistId(i) == m_selectedArtistCopy.id)
                        {
                            // already tracking the current artist, just jump to the next
                            selectedArtistId = song->GetArtistId((i + 1) % e);
                            break;
                        }
                        else if (i == 0)
                            selectedArtistId = song->GetArtistId(0);
                    }

                    auto trackedArtistIndex = m_artists.FindIf<int32_t>([this, selectedArtistId](auto& entry)
                    {
                        return entry == selectedArtistId;
                    });
                    if (trackedArtistIndex >= 0)
                    {
                        float trackingPos = clipper.StartPosY + clipper.ItemsHeight * trackedArtistIndex;
                        ImGui::SetScrollFromPosY(trackingPos - ImGui::GetWindowPos().y);

                        selectedArtist = m_db[selectedArtistId];
                        if (m_selectedArtistCopy.id != selectedArtistId)
                        {
                            m_dbSongsRevision = m_db.SongsRevision() + 1;
                            selectedArtist->CopyTo(&m_selectedArtistCopy);
                        }
                    }

                    m_isTrackingArtist = false;
                }

                ImGui::EndTable();
            }

            ImGui::TableNextColumn();

            // display the artist properties and songs
            if (ImGui::BeginChild("artist", ImVec2(0.0f, 0.0f)) && selectedArtist)
            {
                HandleUI();
                AliasUI();
                NameUI();
                GroupUI();
                CountryUI();
                SaveChangesUI(selectedArtist);
                SourcesUI(selectedArtist);
                MergeUI();
                SongUI();
            }
            ImGui::EndChild();

            ImGui::EndTable();

            // process the merge if any
            if (m_artistMerger.isActive && m_artistMerger.masterArtistId != ArtistID::Invalid)
                ImGui::OpenPopup("Merge Artist");
            ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
            if (ImGui::BeginPopupModal("Merge Artist", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                m_artistMerger.isActive = false;
                auto mergedArtistId = m_artistMerger.mergedArtistId;
                auto masterArtistId = m_artistMerger.masterArtistId;
                auto* masterArtist = m_db[masterArtistId];
                auto* mergedArtist = m_db[mergedArtistId];
                ImGui::Text("Discard: %s / %04X\nTarget: %s / %04X", mergedArtist->GetHandle(), static_cast<uint32_t>(mergedArtistId)
                    , masterArtist->GetHandle(), static_cast<uint32_t>(masterArtistId));
                if (ImGui::Button("Proceed", ImVec2(120, 0)))
                {
                    auto* masterArtistSheet = masterArtist->Edit();

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
                                    masterArtistSheet->numSongs--;
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
                                    oldFilename = m_db.GetFullpath(song);
                                if (dupArtistPos != song->NumArtistIds())
                                {
                                    song->Edit()->artistIds.RemoveAt(i);
                                    break;
                                }
                                else
                                {
                                    masterArtistSheet->numSongs++;
                                    dupArtistPos = i;
                                    auto* songSheet = song->Edit();
                                    songSheet->artistIds[i] = masterArtistId;
                                }
                            }
                        }
                        if (!oldFilename.empty())
                            m_db.Move(oldFilename, song, "MergeArtist");
                    }

                    if (masterArtistSheet->realName.IsEmpty())
                        masterArtistSheet->realName = mergedArtist->GetRealName();
                    for (uint32_t j = 0, mergedSize = mergedArtist->NumHandles(), masterSize = masterArtistSheet->handles.NumItems(); j < mergedSize; j++)
                    {
                        for (uint32_t i = 0;; ++i)
                        {
                            if (i == masterSize)
                            {
                                masterArtistSheet->handles.Add(mergedArtist->GetHandle(j));
                                break;
                            }
                            if (strcmp(mergedArtist->GetHandle(j), masterArtistSheet->handles[i].Items()) == 0)
                                break;
                        }
                    }
                    for (uint32_t j = 0, mergedSize = mergedArtist->NumCountries(), masterSize = masterArtistSheet->countries.NumItems(); j < mergedSize; j++)
                    {
                        for (uint32_t i = 0;; ++i)
                        {
                            if (i == masterSize)
                            {
                                masterArtistSheet->countries.Add(mergedArtist->GetCountry(j));
                                break;
                            }
                            if (mergedArtist->GetCountry(j) == masterArtistSheet->countries[i])
                                break;
                        }
                    }
                    for (uint32_t j = 0, mergedSize = mergedArtist->NumGroups(), masterSize = masterArtistSheet->groups.NumItems(); j < mergedSize; j++)
                    {
                        for (uint32_t i = 0;; ++i)
                        {
                            if (i == masterSize)
                            {
                                masterArtistSheet->groups.Add(mergedArtist->GetGroup(j));
                                break;
                            }
                            if (strcmp(mergedArtist->GetGroup(j), masterArtistSheet->groups[i].Items()) == 0)
                                break;
                        }
                    }
                    if (m_databaseId == DatabaseID::kLibrary)
                    {
                        for (auto& source : mergedArtist->Sources())
                            masterArtistSheet->sources.Add(source);
                    }

                    m_db.RemoveArtist(mergedArtist->GetId());

                    m_db.Raise(Database::Flag::kSaveArtists | Database::Flag::kSaveSongs);

                    m_selectedArtistCopy = {};
                    m_artistMerger.masterArtistId = ArtistID::Invalid;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    m_artistMerger.masterArtistId = ArtistID::Invalid;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    Array<DatabaseSongsUI::SubsongEntry> DatabaseArtistsUI::GatherEntries() const
    {
        Array<DatabaseSongsUI::SubsongEntry> entries;
        auto selectedArtistId = m_selectedArtistCopy.id;
        Artist* selectedArtist = m_db.IsValid(selectedArtistId) ? m_db[selectedArtistId] : nullptr;
        if (selectedArtist)
        {
            for (auto* song : m_db.Songs())
            {
                for (auto artistId : song->ArtistIds())
                {
                    if (artistId == selectedArtistId)
                    {
                        for (uint16_t i = 0, lastSubsongIndex = song->GetLastSubsongIndex(); i <= lastSubsongIndex; i++)
                        {
                            if (!song->IsSubsongDiscarded(i))
                            {
                                SubsongEntry subsongEntry;
                                subsongEntry.songId = song->GetId();
                                subsongEntry.index = i;
                                entries.Add(subsongEntry);
                            }
                        }
                        break;
                    }
                }
            }
        }
        return entries;
    }

    void DatabaseArtistsUI::SourcesUI(Artist* selectedArtist)
    {
        UnusedArg(selectedArtist);
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

    ArtistID DatabaseArtistsUI::PickArtist(ArtistID skippedArtistId)
    {
        if (m_artistPicker.filter->Draw("##filter", -1) || !m_artistPicker.isOpened || m_artistPicker.revision != m_dbArtistsRevision)
        {
            m_artistPicker.artists.Clear();
            m_artistPicker.artists.Reserve(m_db.NumArtists());

            for (auto artist : m_db.Artists())
            {
                if (artist->GetId() != skippedArtistId && m_artistPicker.filter->PassFilter(artist->GetHandle()))
                    m_artistPicker.artists.Add(artist->GetId());
            }
            m_artistPicker.isOpened = true;
            m_artistPicker.revision = m_dbArtistsRevision;
        }

        ArtistID pickedArtistId = ArtistID::Invalid;
        ImGuiListClipper clipper;
        clipper.Begin(m_artistPicker.artists.NumItems<int32_t>());
        while (clipper.Step())
        {
            for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
            {
                auto* artist = m_db[m_artistPicker.artists[rowIdx]];
                ImGui::PushID(static_cast<int>(artist->GetId()));
                std::string label = artist->GetHandle();
                if (artist->NumSources() != 0)
                {
                    label += " / ";
                    label += artist->GetSource(0).GetName();
                }
                if (ImGui::MenuItem(label.c_str()))
                    pickedArtistId = artist->GetId();
                ImGui::PopID();
            }
        }

        return pickedArtistId;
    }

    void DatabaseArtistsUI::OnArtistFilterLoaded(uintptr_t userData, void*, const char*)
    {
        auto* artistFilter = reinterpret_cast<ImGuiTextFilter*>(userData);
        artistFilter->Build();
    }

    void DatabaseArtistsUI::OnArtistSelectionLoaded(uintptr_t userData, void*, const char*)
    {
        auto* ui = reinterpret_cast<DatabaseArtistsUI*>(userData);
        Artist* selectedArtist = ui->m_db.IsValid(ui->m_selectedArtistCopy.id) ? ui->m_db[ui->m_selectedArtistCopy.id] : nullptr;
        if (selectedArtist)
        {
            selectedArtist->CopyTo(&ui->m_selectedArtistCopy);
        }
        else
            ui->m_selectedArtistCopy = {};
    }

    void DatabaseArtistsUI::HandleUI()
    {
        char label[sizeof("Handle for ID:0000")];
        sprintf(label, "Handle for ID:%04X", static_cast<uint32_t>(m_selectedArtistCopy.id));

        ImGui::SeparatorText(label);

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##Handle", &m_selectedArtistCopy.handles[0].String(), 0);
    }

    void DatabaseArtistsUI::AliasUI()
    {
        ImGui::SeparatorText("Alias");

        const float button_size = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();

        auto aliasButton = [&]()
        {
            if (ImGui::Button("+", ImVec2(button_size, button_size)))
                m_selectedArtistCopy.handles.Add("");
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Add another alias");
        };

        if (m_selectedArtistCopy.handles.NumItems() == 1)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushID(-1);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Max(1.0f, ImGui::CalcItemWidth() - button_size));
            aliasButton();
            ImGui::PopID();
        }
        else
        {
            int32_t handleToSwap = 0;
            int32_t handleToRemove = 0;
            for (int32_t i = 1, last = m_selectedArtistCopy.handles.NumItems() - 1; i <= last; i++)
            {
                bool isLast = i == last;

                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * (isLast ? 3 : 2)));
                ImGui::InputText("##Handle", &m_selectedArtistCopy.handles[i].String());
                if (isLast)
                {
                    ImGui::SameLine(0, style.ItemInnerSpacing.x);
                    aliasButton();
                }
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
        }
    }

    void DatabaseArtistsUI::NameUI()
    {
        ImGui::SeparatorText("Name");

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##Name", &m_selectedArtistCopy.realName.String());
    }

    void DatabaseArtistsUI::GroupUI()
    {
        ImGui::SeparatorText("Group");

        ImGui::PushID("EditGroups");
        const float button_size = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();

        auto groupButton = [&]()
        {
            if (ImGui::Button("+", ImVec2(button_size, button_size)))
                m_selectedArtistCopy.groups.Add("");
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Add another group");
        };

        if (m_selectedArtistCopy.groups.IsEmpty())
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushID(-1);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Max(1.0f, ImGui::CalcItemWidth() - button_size));
            groupButton();
            ImGui::PopID();
        }
        else
        {
            int32_t groupToRemove = -1;
            for (int32_t i = 0, last = m_selectedArtistCopy.groups.NumItems() - 1; i <= last; i++)
            {
                bool isLast = i == last;
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * (isLast ? 2 : 1)));
                ImGui::InputText("##Group", &m_selectedArtistCopy.groups[i].String());
                if (isLast)
                {
                    ImGui::SameLine(0, style.ItemInnerSpacing.x);
                    groupButton();
                }
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                if (ImGui::Button("X", ImVec2(button_size, button_size)))
                    groupToRemove = i;
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Remove group");
                ImGui::PopID();
            }
            if (groupToRemove >= 0)
                m_selectedArtistCopy.groups.RemoveAt(groupToRemove);
        }
        ImGui::PopID();
    }

    void DatabaseArtistsUI::CountryUI()
    {
        ImGui::SeparatorText("Country");

        ImGui::PushID("EditCountries");
        const float button_size = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();

        auto countriyButton = [&]()
        {
            if (ImGui::Button("+", ImVec2(button_size, button_size)))
                ImGui::OpenPopup("StatePopup");
            ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
            if (ImGui::BeginPopup("StatePopup"))
            {
                auto countries = Countries::GetComboList(m_selectedArtistCopy.countries.Container());
                ImGuiListClipper clipper;
                clipper.Begin(countries.NumItems<int32_t>());
                while (clipper.Step())
                {
                    for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                    {
                        if (ImGui::Selectable(countries[rowIdx]))
                            m_selectedArtistCopy.countries.Add(Countries::GetCode(countries[rowIdx]));
                    }
                }

                ImGui::EndPopup();
            }
            else if (ImGui::IsItemHovered())
                ImGui::Tooltip("Add another country");
        };

        if (m_selectedArtistCopy.countries.IsEmpty())
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushID(-1);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Max(1.0f, ImGui::CalcItemWidth() - button_size));
            countriyButton();
            ImGui::PopID();
        }
        else
        {
            int32_t countryToRemove = -1;
            for (int32_t i = 0, last = m_selectedArtistCopy.countries.NumItems() - 1; i <= last; i++)
            {
                bool isLast = i == last;
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(m_selectedArtistCopy.countries[i]);
                ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * (isLast ? 2 : 1)));
                ImGui::InputText("##country", const_cast<char*>(Countries::GetName(m_selectedArtistCopy.countries[i])), strlen(Countries::GetName(m_selectedArtistCopy.countries[i])) + 1, ImGuiInputTextFlags_ReadOnly);
                if (isLast)
                {
                    ImGui::SameLine(0, style.ItemInnerSpacing.x);
                    countriyButton();
                }
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
                if (ImGui::Button("X", ImVec2(button_size, button_size)))
                    countryToRemove = i;
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Remove country");
                ImGui::PopID();
            }
            if (countryToRemove >= 0)
                m_selectedArtistCopy.countries.RemoveAt(countryToRemove);
        }
        ImGui::PopID();
    }

    void DatabaseArtistsUI::SaveChangesUI(Artist* selectedArtist)
    {
        ImGui::SeparatorText("");
        auto isUpdated = !selectedArtist->AreSameHandles(&m_selectedArtistCopy);
        isUpdated |= selectedArtist->Countries() != m_selectedArtistCopy.countries;
        isUpdated |= !selectedArtist->AreSameGroups(&m_selectedArtistCopy);
        isUpdated |= selectedArtist->GetRealName() != m_selectedArtistCopy.realName;

        ImGui::BeginDisabled(!isUpdated);
        if (ImGui::Button("Save Changes", ImVec2(-FLT_MIN, 0)))
            OnSavedChanges(selectedArtist);
        ImGui::EndDisabled();
    }

    void DatabaseArtistsUI::MergeUI()
    {
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 1.0f));
        ImGui::BeginDisabled(m_db.NumArtists() <= 1);
        if (ImGui::Button("Merge With Another Artist", ImVec2(-FLT_MIN, 0)))
            ImGui::OpenPopup("MergeArtist");
        ImGui::PopStyleColor(3);
        ImGui::SetNextWindowSizeConstraints(ImVec2(128.0f, ImGui::GetTextLineHeightWithSpacing()), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
        if (ImGui::BeginPopupContextItem("MergeArtist"))
        {
            m_artistMerger.mergedArtistId = m_selectedArtistCopy.id;
            m_artistMerger.masterArtistId = PickArtist(m_artistMerger.mergedArtistId);
            if (m_artistMerger.masterArtistId != ArtistID::Invalid)
                m_artistMerger.isActive = true;
            ImGui::EndPopup();
        }
        else
            m_artistPicker.isOpened = false;
        ImGui::EndDisabled();
    }

    void DatabaseArtistsUI::SongUI()
    {
        ImGui::SeparatorText("Songs");

        DatabaseSongsUI::OnDisplay();
    }
}
// namespace rePlayer