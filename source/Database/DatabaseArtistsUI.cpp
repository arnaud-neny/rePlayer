#include "DatabaseArtistsUI.h"

// Core
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>
#include <IO/File.h>

// rePlayer
#include <Database/Types/Countries.h>
#include <Database/Database.h>
#include <Database/DatabaseSongsUI.h>
#include <Database/SongEditor.h>
#include <RePlayer/Core.h>

// stl
#include <algorithm>

namespace rePlayer
{
    inline constexpr DatabaseArtistsUI::States& operator|=(DatabaseArtistsUI::States& a, DatabaseArtistsUI::States b)
    {
        a = DatabaseArtistsUI::States(uint32_t(a) | uint32_t(b));
        return a;
    }

    inline constexpr bool operator&&(DatabaseArtistsUI::States a, DatabaseArtistsUI::States b)
    {
        return uint32_t(a) & uint32_t(b);
    }

    inline bool DatabaseArtistsUI::SongEntry::operator==(SongID otherId) const
    {
        return id == otherId;
    }

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
                        if (isSelected = ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastTopHeadersRowHeight)))//TBD: using imgui_internal for row height
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

            if (m_artistMerger.isActive && m_artistMerger.masterArtistId != ArtistID::Invalid)
                ImGui::OpenPopup("Merge Artist");
            ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
            if (ImGui::BeginPopupModal("Merge Artist", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                auto mergedArtistId = m_artistMerger.mergedArtistId;
                auto masterArtistId = m_artistMerger.masterArtistId;
                auto* masterArtist = m_db[masterArtistId];
                auto* mergedArtist = m_db[mergedArtistId];
                ImGui::Text("Discard: %s / %04X\nTarget: %s / %04X", mergedArtist->GetHandle(), static_cast<uint32_t>(mergedArtistId)
                    , masterArtist->GetHandle(), static_cast<uint32_t>(masterArtistId));
                if (ImGui::Button("Proceed", ImVec2(120, 0)))
                {
                    auto* songs = m_db.GetSongsUI();
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
                                    oldFilename = songs->GetFullpath(song);
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
                        {
                            auto newFilename = songs->GetFullpath(song);
                            io::File::Move(oldFilename.c_str(), newFilename.c_str());
                        }
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
                    m_selectedCountry = -1;
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
                m_artistMerger.mergedArtistId = selectedArtist->GetId();
                if (SelectMasterArtist(m_artistMerger.mergedArtistId))
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
        auto dbSongsRevision = m_db.SongsRevision();
        if (selectedArtist)
        {
            bool isDirty = false;
            auto selectedArtistId = selectedArtist->GetId();
            ImGui::Text("%u", selectedArtist->NumSongs());
            if (m_selectedArtist != selectedArtistId)
            {
                m_songEntries.Clear();
                m_songEntries.Reserve(selectedArtist->NumSongs());
                for (auto* song : m_db.Songs())
                {
                    for (auto artistId : song->ArtistIds())
                    {
                        if (artistId == selectedArtistId)
                        {
                            m_songEntries.Add({ song->GetId() });
                            break;
                        }
                    }
                }
                m_selectedArtist = selectedArtistId;
                isDirty = true;
                m_numSelectedSongEntries = 0;
            }
            else if (dbSongsRevision != m_dbSongsRevision)
            {
                m_numSelectedSongEntries = 0;
                auto oldEntries = std::move(m_songEntries);
                m_songEntries.Reserve(selectedArtist->NumSongs());
                for (auto* song : m_db.Songs())
                {
                    for (auto artistId : song->ArtistIds())
                    {
                        if (artistId == selectedArtistId)
                        {
                            m_songEntries.Add({ song->GetId() });
                            oldEntries.Remove(song->GetId(), 0, [&](auto& oldEntry)
                            {
                                if (oldEntry.isSelected)
                                {
                                    m_songEntries.Last().isSelected = true;
                                    m_numSelectedSongEntries++;
                                }
                            });
                            break;
                        }
                    }
                }
                isDirty = true;
            }

            auto states = States::kNone;

            constexpr ImGuiTableFlags flags =
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
                | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings; // todo remove nosave

            if (ImGui::BeginTable("artistSongs", kNumIDs, flags))
            {
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.0f, kTitle);
                ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthStretch, 0.0f, kArtists);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 0.0f, kType);
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                SortSongs(isDirty);

                ImGuiListClipper clipper;
                clipper.Begin(m_songEntries.NumItems<int>());

                auto musicId = MusicID({}, m_databaseId);
                for (uint32_t i = 0, e = m_songEntries.NumItems(); i < e; i++)
                {
                    musicId.subsongId.songId = m_songEntries[i].id;
                    Song* song = m_db[musicId.subsongId];

                    ImGui::PushID(&musicId.subsongId, &musicId.subsongId + 1);
                    ImGui::TableNextColumn();
                    states |= Selection(i, musicId, selectedArtist);
                    ImGui::SameLine(0.0f, 0.0f);//no spacing
                    ImGui::TextUnformatted(song->GetName());

                    ImGui::TableNextColumn();
                    musicId.DisplayArtists();

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(song->GetType().GetExtension());

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            if (states && States::kRemoveArtist)
                RemoveArtist(selectedArtist);
        }
        else
        {
            m_selectedArtist = {};
            m_songEntries.Clear();
            m_numSelectedSongEntries = 0;
        }
        m_dbSongsRevision = dbSongsRevision;
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
                    m_artistMerger.masterArtistId = artist->GetId();
            }
        }

        return m_artistMerger.masterArtistId != ArtistID::Invalid;
    }

    void DatabaseArtistsUI::SortSongs(bool isDirty)
    {
        auto* sortsSpecs = ImGui::TableGetSortSpecs();
        if (sortsSpecs && (sortsSpecs->SpecsDirty || isDirty) && m_songEntries.NumItems() > 1)
        {
            std::sort(m_songEntries.begin(), m_songEntries.end(), [this, sortsSpecs](auto& l, auto& r)
            {
                Song* lSong = m_db[l.id];
                Song* rSong = m_db[r.id];
                for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                {
                    auto& sortSpec = sortsSpecs->Specs[i];
                    int64_t delta = 0;
                    switch (sortSpec.ColumnUserID)
                    {
                    case kTitle:
                        delta = CompareStringMixed(lSong->GetName(), rSong->GetName());
                        break;
                    case kArtists:
                        delta = CompareStringMixed(m_db.GetArtists(l.id).c_str(), m_db.GetArtists(r.id).c_str());
                        break;
                    case kType:
                        delta = strcmp(lSong->GetType().GetExtension(), rSong->GetType().GetExtension());
                        break;
                    }

                    if (delta)
                        return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                }
                return l.id < r.id;
            });

            sortsSpecs->SpecsDirty = false;
        }
    }

    DatabaseArtistsUI::States DatabaseArtistsUI::Selection(int32_t entryIdx, MusicID musicId, Artist* selectedArtist)
    {
        auto states = States::kNone;

        // Update the selection
        bool isSelected = m_songEntries[entryIdx].isSelected;
        if (ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastTopHeadersRowHeight)))//tbd: using imgui_internal for row height
            isSelected = Select(entryIdx, musicId, isSelected);
        // Open song editor with middle button
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        {
            Core::GetSongEditor().OnSongSelected(musicId);
            Core::GetSongEditor().Enable(true);
        }
        // Drag selected files
        if (ImGui::BeginDragDropSource())
        {
            if (!isSelected)
            {
                m_songEntries[entryIdx].isSelected = isSelected = true;
                m_numSelectedSongEntries++;
            }
            DragSelection();
            ImGui::EndDragDropSource();
        }
        // Context menu
        if (ImGui::BeginPopupContextItem("Song Popup"))
        {
            if (!isSelected)
            {
                m_songEntries[entryIdx].isSelected = isSelected = true;
                m_numSelectedSongEntries++;
            }
            states |= SelectionContext(isSelected, selectedArtist);
            ImGui::EndPopup();
        }

        return states;
    }

    void DatabaseArtistsUI::RemoveArtist(Artist* selectedArtist)
    {
        auto* songs = m_db.GetSongsUI();
        for (auto& entry : m_songEntries)
        {
            if (entry.isSelected)
            {
                selectedArtist->numSongs--;

                auto* song = m_db[entry.id];
                auto* songSheet = song->Edit();
                auto oldFilename = songs->GetFullpath(song);
                songSheet->artistIds.Remove(selectedArtist->id);
                if (m_databaseId == DatabaseID::kLibrary && !oldFilename.empty())
                {
                    auto newFilename = songs->GetFullpath(song);
                    io::File::Move(oldFilename.c_str(), newFilename.c_str());
                }
            }
        }

        if (selectedArtist->numSongs == 0)
        {
            m_db.RemoveArtist(selectedArtist->id);
            m_selectedArtistCopy = {};
            m_selectedArtistCopy.handles.Push();
            m_selectedArtist = ArtistID::Invalid;
        }
        m_lastSelectedSong = SongID::Invalid;
        m_songEntries.Clear();
        m_numSelectedSongEntries = 0;
        m_db.Raise(Database::Flag::kSaveArtists | Database::Flag::kSaveSongs);
    }

    bool DatabaseArtistsUI::Select(int32_t entryIdx, MusicID musicId, bool isSelected)
    {
        Core::GetSongEditor().OnSongSelected(musicId);

        if (ImGui::GetIO().KeyShift)
        {
            if (!ImGui::GetIO().KeyCtrl)
            {
                for (auto& entry : m_songEntries)
                    entry.isSelected = false;
            }
            auto lastSelectedIndex = m_songEntries.FindIf<int32_t>([this](auto& entry)
            {
                return entry.id == m_lastSelectedSong;
            });
            if (lastSelectedIndex < 0)
            {
                lastSelectedIndex = entryIdx;
                m_lastSelectedSong = musicId.subsongId.songId;
            }
            if (lastSelectedIndex <= entryIdx) for (; lastSelectedIndex <= entryIdx; lastSelectedIndex++)
                m_songEntries[lastSelectedIndex].isSelected = true;
            else for (int32_t i = entryIdx; i <= lastSelectedIndex; i++)
                m_songEntries[i].isSelected = true;
            isSelected = true;
            m_numSelectedSongEntries = 0;
            for (auto& entry : m_songEntries)
            {
                if (entry.isSelected)
                    m_numSelectedSongEntries++;
            }
        }
        else
        {
            m_lastSelectedSong = musicId.subsongId.songId;
            if (ImGui::GetIO().KeyCtrl)
            {
                m_songEntries[entryIdx].isSelected = !m_songEntries[entryIdx].isSelected;
                isSelected ? m_numSelectedSongEntries-- : m_numSelectedSongEntries++;
            }
            else
            {
                for (auto& entry : m_songEntries)
                    entry.isSelected = false;
                m_songEntries[entryIdx].isSelected = !isSelected;
                m_numSelectedSongEntries = isSelected ? 1 : 0;
            }
            isSelected = !isSelected;
        }
        return isSelected;
    }

    void DatabaseArtistsUI::DragSelection()
    {
        MusicID musicId({}, m_databaseId);
        Array<MusicID> subsongsList;
        for (auto& entry : m_songEntries)
        {
            if (entry.isSelected)
            {
                musicId.subsongId.songId = entry.id;
                auto* song = m_db[entry.id];
                for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                {
                    if (!song->IsSubsongDiscarded(i))
                    {
                        musicId.subsongId.index = i;
                        subsongsList.Add(musicId);
                    }
                }
            }
        }
        ImGui::SetDragDropPayload("DATABASE", subsongsList.begin(), subsongsList.Size<size_t>());

        if (subsongsList.NumItems() <= 7)
        {
            for (uint32_t i = 0; i < 7 && i < subsongsList.NumItems(); i++)
                ImGui::TextUnformatted(m_db[subsongsList[i].subsongId]->GetName());
        }
        else
        {
            ImGui::TextUnformatted(m_db[subsongsList[0].subsongId]->GetName());
            ImGui::TextUnformatted(m_db[subsongsList[1].subsongId]->GetName());
            ImGui::Text("\n... %d songs ...\n ", subsongsList.NumItems() - 4);
            ImGui::TextUnformatted(m_db[subsongsList[subsongsList.NumItems() - 2].subsongId]->GetName());
            ImGui::TextUnformatted(m_db[subsongsList[subsongsList.NumItems() - 1].subsongId]->GetName());
        }
    }

    DatabaseArtistsUI::States DatabaseArtistsUI::SelectionContext(bool isSelected, Artist* selectedArtist)
    {
        auto states = States::kNone;
        if (ImGui::Selectable("Invert selection"))
        {
            for (auto& entry : m_songEntries)
                entry.isSelected = !entry.isSelected;
            isSelected = !isSelected;
            m_numSelectedSongEntries = m_songEntries.NumItems() - m_numSelectedSongEntries;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Add to playlist"))
        {
            MusicID musicId({}, m_databaseId);
            for (auto& entry : m_songEntries)
            {
                if (entry.isSelected)
                {
                    musicId.subsongId.songId = entry.id;
                    auto* song = m_db[entry.id];
                    for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                    {
                        if (!song->IsSubsongDiscarded(i))
                        {
                            musicId.subsongId.index = i;
                            Core::Enqueue(musicId);
                        }
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::BeginMenu("Remove from artist"))
        {
            if (ImGui::Selectable(selectedArtist->GetHandle(), false))
                states |= States::kRemoveArtist;
            ImGui::EndMenu();
        }
        return states;
    }
}
// namespace rePlayer