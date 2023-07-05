#include "DatabaseSongsUI.h"

// Core
#include <ImGui.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ImGui/imgui_internal.h>

// rePlayer
#include <Database/Database.h>
#include <Database/SongEditor.h>
#include <Deck/Deck.h>
#include <RePlayer/Core.h>
#include <RePlayer/Export.h>

// Windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// stl
#include <algorithm>
#include <chrono>
#include <cmath>

namespace rePlayer
{
    inline bool DatabaseSongsUI::SubsongEntry::operator==(SubsongID otherId) const
    {
        return id == otherId;
    }

    DatabaseSongsUI::DatabaseSongsUI(DatabaseID databaseId, Window& owner)
        : m_db(Core::GetDatabase(databaseId))
        , m_owner(owner)
        , m_databaseId(databaseId)
        , m_songFilter(new ImGuiTextFilter())
    {
        m_db.Register(this);
        owner.RegisterSerializedData(m_filterMode, "SongsFilterMode");
        owner.RegisterSerializedData(m_songFilter->InputBuf, "SongsFilter", &OnSongFilterLoaded, uintptr_t(m_songFilter));
    }

    DatabaseSongsUI::~DatabaseSongsUI()
    {
        if (m_export)
        {
            m_export->Cancel();
            while (!m_export->IsDone())
                ::Sleep(1);
            delete m_export;
        }
        delete m_songFilter;
    }

    void DatabaseSongsUI::TrackSubsong(SubsongID subsongId)
    {
        m_trackedSubsongId = subsongId;
        if (!m_owner.IsVisible())
            m_subsongHighlights.RemoveAll();
        m_subsongHighlights[subsongId] = 1.0f;
    }

    void DatabaseSongsUI::OnDisplay()
    {
        bool isDirty = m_dbRevision != m_db.SongsRevision();
        m_dbRevision = m_db.SongsRevision();

        FilteringUI(isDirty);
        SongsUI(isDirty);
        ExportAsWavUI();
    }

    void DatabaseSongsUI::OnEndUpdate()
    {
        for (auto it = m_subsongHighlights.begin(); it != m_subsongHighlights.end();)
        {
            static constexpr float kFadeOutSpeed = 1.0f;
            it.Item() -= ImGui::GetIO().DeltaTime * kFadeOutSpeed;
            if (it.Item() <= 0.f)
                m_subsongHighlights.RemoveAndStepIteratorForward(it);
            else
                ++it;
        }
    }

    void DatabaseSongsUI::OnAddingArtistToSong(Song* song, ArtistID artistId)
    {
        song->Edit()->artistIds.Add(artistId);
    }

    void DatabaseSongsUI::OnSelectionContext()
    {}

    void DatabaseSongsUI::OnSongFilterLoaded(uintptr_t userData, void*, const char*)
    {
        auto* songFilter = reinterpret_cast<ImGuiTextFilter*>(userData);
        songFilter->Build();
    }

    void DatabaseSongsUI::FilteringUI(bool& isDirty)
    {
        if (ImGui::BeginTable("Menu", 2, ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
            ImGui::TableSetupColumn("Filter", ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
            ImGui::TableNextColumn();

            if (ImGui::Button("Tags"))
                ImGui::OpenPopup("FilterTags");
            if (ImGui::BeginPopup("FilterTags"))
            {
                static const char* const item[] = {
                    "Disable###Mode",
                    "Match###Mode",
                    "Inclusive###Mode",
                    "Exclusive###Mode"
                };
                if (ImGui::BeginMenu(item[static_cast<int>(m_tagMode)]))
                {
                    auto tagMode = m_tagMode;
                    for (uint32_t i = 0; i < 4; i++)
                    {
                        if (tagMode != static_cast<TagMode>(i) && ImGui::MenuItem(item[i]))
                        {
                            isDirty = true;
                            m_tagMode = static_cast<TagMode>(i);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                for (uint32_t i = 0; i < Tag::kNumTags; i++)
                {
                    bool isChecked = m_filterTags.IsEnabled(1ull << i);
                    if (ImGui::Checkbox(Tag::Name(i), &isChecked))
                    {
                        m_filterTags.Enable(1ull << i, isChecked);
                        isDirty |= m_tagMode != TagMode::Disable;
                    }
                }

                ImGui::EndPopup();
            }
            ImGui::SameLine();
            auto oldFilterMode = m_filterMode;
            ImGui::Combo("##Filter", reinterpret_cast<int*>(&m_filterMode), "All\0Songs\0Artists\0");
            isDirty |= oldFilterMode != m_filterMode;
            ImGui::TableNextColumn();

            auto tagMode = m_tagMode;
            bool isFilteringTags = tagMode != TagMode::Disable;
            bool isFiltering = isDirty && (m_songFilter->IsActive() || isFilteringTags);
            if (m_songFilter->Draw("##filter", -1) || isFiltering)
            {
                m_numSelectedEntries = 0;
                auto oldEntries = std::move(m_entries);
                bool isFilteringSong = m_filterMode != FilterMode::Artists;
                auto filterTags = m_filterTags;
                if (m_filterMode != FilterMode::Songs)
                {
                    Array<ArtistID> artists;
                    for (Artist* artist : m_db.Artists())
                    {
                        if (m_songFilter->PassFilter(artist->GetHandle()))
                            artists.Add(artist->GetId());
                    }
                    for (auto* song : m_db.Songs())
                    {
                        if (isFilteringTags)
                        {
                            auto songTags = song->GetTags();
                            if (tagMode == TagMode::Match && songTags != filterTags)
                                continue;
                            else if (tagMode == TagMode::Exclusive && !songTags.IsEnabled(filterTags))
                                continue;
                            else if (tagMode == TagMode::Inclusive && !songTags.IsAnyEnabled(filterTags))
                                continue;
                        }
                        bool checkForSong = isFilteringSong;
                        if (song->NumArtistIds() == 0 && m_songFilter->PassFilter(nullptr, nullptr))
                        {
                            auto songId = song->GetId();
                            for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                            {
                                if (!song->IsSubsongDiscarded(i))
                                {
                                    m_entries.Add(SubsongID(songId, i));
                                    oldEntries.Remove(SubsongID(songId, i), 0, [&](auto& oldEntry)
                                    {
                                        if (oldEntry.isSelected)
                                        {
                                            m_entries.Last().isSelected = true;
                                            m_numSelectedEntries++;
                                        }
                                    });
                                }
                            }
                            checkForSong = false;
                        }
                        else for (auto artistId : song->ArtistIds())
                        {
                            if (artists.Find(artistId))
                            {
                                auto songId = song->GetId();
                                for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                                {
                                    if (!song->IsSubsongDiscarded(i))
                                    {
                                        m_entries.Add(SubsongID(songId, i));
                                        oldEntries.Remove(SubsongID(songId, i), 0, [&](auto& oldEntry)
                                        {
                                            if (oldEntry.isSelected)
                                            {
                                                m_entries.Last().isSelected = true;
                                                m_numSelectedEntries++;
                                            }
                                        });
                                    }
                                }
                                checkForSong = false;
                                break;
                            }
                        }
                        if (checkForSong && m_songFilter->PassFilter(song->GetName()))
                        {
                            auto songId = song->GetId();
                            for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                            {
                                if (!song->IsSubsongDiscarded(i))
                                {
                                    m_entries.Add(SubsongID(songId, i));
                                    oldEntries.Remove(SubsongID(songId, i), 0, [&](auto& oldEntry)
                                    {
                                        if (oldEntry.isSelected)
                                        {
                                            m_entries.Last().isSelected = true;
                                            m_numSelectedEntries++;
                                        }
                                    });
                                }
                            }
                        }
                    }
                }
                else
                {
                    for (auto* song : m_db.Songs())
                    {
                        if (isFilteringTags)
                        {
                            auto songTags = song->GetTags();
                            if (tagMode == TagMode::Match && songTags != filterTags)
                                continue;
                            else if (tagMode == TagMode::Exclusive && !songTags.IsEnabled(filterTags))
                                continue;
                            else if (tagMode == TagMode::Inclusive && !songTags.IsAnyEnabled(filterTags))
                                continue;
                        }
                        if (m_songFilter->PassFilter(song->GetName()))
                        {
                            auto songId = song->GetId();
                            for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                            {
                                if (!song->IsSubsongDiscarded(i))
                                {
                                    m_entries.Add(SubsongID(songId, i));
                                    oldEntries.Remove(SubsongID(songId, i), 0, [&](auto& oldEntry)
                                    {
                                        if (oldEntry.isSelected)
                                        {
                                            m_entries.Last().isSelected = true;
                                            m_numSelectedEntries++;
                                        }
                                    });
                                }
                            }
                        }
                    }
                }
                isDirty = true;
            }
            else if (isDirty)
            {
                m_numSelectedEntries = 0;
                auto oldEntries = std::move(m_entries);
                for (auto* song : m_db.Songs())
                {
                    auto songId = song->GetId();
                    for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                    {
                        if (!song->IsSubsongDiscarded(i))
                        {
                            m_entries.Add(SubsongID(songId, i));
                            oldEntries.Remove(SubsongID(songId, i), 0, [&](auto& oldEntry)
                            {
                                if (oldEntry.isSelected)
                                {
                                    m_entries.Last().isSelected = true;
                                    m_numSelectedEntries++;
                                }
                            });
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
    }

    void DatabaseSongsUI::SongsUI(bool isDirty)
    {
        // Create a child to be able to detect the focus and do the Ctrl-A
        ImGui::BeginChild("SongsChild");

        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
            | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable;

        if (ImGui::BeginTable("songs", kNumIDs, flags))
        {
            ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.0f, kTitle);
            ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthStretch, 0.0f, kArtists);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 0.0f, kType);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 0.0f, kSize);
            ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 0.0f, kDuration);
            ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 0.0f, kYear);
            ImGui::TableSetupColumn("CRC", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_DefaultHide, 0.0f, kCRC);
            ImGui::TableSetupColumn(">|", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 0.0f, kState);
            ImGui::TableSetupColumn("[%]", ImGuiTableColumnFlags_WidthFixed, 0.0f, kRating);
            ImGui::TableSetupColumn("Added", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 0.0f, kDatabaseDate);
            ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
            ImGui::TableHeadersRow();

            SortSubsongs(isDirty);

            SubsongID currentRatingSubsong;

            MusicID currentPlayingSong = Core::GetDeck().GetCurrentPlayerId();

            ImGuiListClipper clipper;
            clipper.Begin(m_entries.NumItems<int>());

            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    auto musicId = MusicID(m_entries[rowIdx].id, m_databaseId);
                    Song* song = m_db[musicId.subsongId];

                    ImGui::PushID(musicId.subsongId.value);
                    ImGui::TableNextColumn();

                    SetRowBackground(rowIdx, song, musicId.subsongId, currentPlayingSong);
                    Selection(rowIdx, musicId);
                    ImGui::SameLine(0.0f, 0.0f);//no spacing
                    ImGui::TextUnformatted(m_db.GetTitle(musicId.subsongId).c_str());
                    if (ImGui::IsItemHovered())
                        musicId.SongTooltip();
                    ImGui::TableNextColumn();
                    musicId.DisplayArtists();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", song->GetType().GetExtension());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", song->GetFileSize());
                    ImGui::TableNextColumn();
                    {
                        auto subsongDurationCs = song->GetSubsongDurationCs(musicId.subsongId.index);
                        ImGui::Text("%u:%02u", subsongDurationCs / 6000, (subsongDurationCs / 100) % 60);
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%04u", song->GetReleaseYear());
                    ImGui::TableNextColumn();
                    ImGui::Text("%08X", song->GetFileCrc());
                    ImGui::TableNextColumn();

                    static const char* const subsongStateLabels[] = { "U", "S", "F", "L" };
                    static const char* const subsongStates[] = { "Undefined", "Stop", "Fade Out", "Loop Once" };
                    auto subsongState = song->GetSubsongState(musicId.subsongId.index);
                    if (ImGui::Button(subsongStateLabels[static_cast<uint32_t>(subsongState)]))
                        ImGui::OpenPopup("StatePopup");
                    if (ImGui::BeginPopup("StatePopup"))
                    {
                        for (uint32_t i = 0; i < kNumSubsongStates; i++)
                        {
                            auto state = static_cast<SubsongState>(i);

                            if (ImGui::Selectable(subsongStates[i], subsongState == state))
                            {
                                if (state != subsongState)
                                {
                                    song->Edit()->subsongs[musicId.subsongId.index].state = state;
                                    m_db.Raise(Database::Flag::kSaveSongs);
                                }
                            }
                        }
                        ImGui::EndPopup();
                    }
                    else if (ImGui::IsItemHovered())
                        ImGui::Tooltip(subsongStates[static_cast<uint32_t>(subsongState)]);

                    ImGui::TableNextColumn();
                    int32_t subsongRating = song->GetSubsongRating(musicId.subsongId.index);
                    if (subsongRating == 0)
                        ImGui::ProgressBar(0.0f, ImVec2(-1.f, 0.f), "N/A");
                    else
                        ImGui::ProgressBar((subsongRating - 1) * 0.01f);
                    if (ImGui::BeginPopupContextItem("Rating", ImGuiPopupFlags_MouseButtonLeft))
                    {
                        char label[8] = "N/A";
                        if (subsongRating > 0)
                            sprintf(label, "%u%%%%", subsongRating - 1);
                        if (ImGui::SliderInt("##rating", &subsongRating, 0, 101, label, ImGuiSliderFlags_NoInput))
                        {
                            if (song->GetSubsongRating(musicId.subsongId.index) != subsongRating)
                            {
                                song->Edit()->subsongs[musicId.subsongId.index].rating = subsongRating;
                                m_db.Raise(Database::Flag::kSaveSongs);
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::TableNextColumn();
                    auto added = std::chrono::year_month_day(std::chrono::sys_days(std::chrono::days(song->GetDatabaseDay() + Core::kReferenceDate)));
                    ImGui::Text("%04d/%02u/%02u", int32_t(added.year()), uint32_t(added.month()), uint32_t(added.day()));
                    ImGui::PopID();
                }
            }

            if (m_trackedSubsongId.IsValid())
            {
                auto trackedSubsongIndex = m_entries.FindIf<int32_t>([this](auto& subsong)
                {
                    return subsong.id == m_trackedSubsongId;
                });
                if (trackedSubsongIndex >= 0)
                {
                    float item_pos_y = clipper.StartPosY + clipper.ItemsHeight * trackedSubsongIndex;
                    ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetWindowPos().y);
                }
                m_trackedSubsongId = {};
            }

            ImGui::EndTable();
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
        {
            m_numSelectedEntries = m_entries.NumItems();
            for (auto& entry : m_entries)
                entry.isSelected = true;
        }
        ImGui::EndChild();
    }

    void DatabaseSongsUI::ExportAsWavUI()
    {
        if (m_isExportAsWavTriggered)
        {
            m_isExportAsWavTriggered = false;
            m_export = new Export();
            for (auto& entry : m_entries)
            {
                if (entry.isSelected)
                    m_export->Enqueue({ entry.id, m_databaseId });
            }
            if (m_export->Start())
            {
                ImGui::OpenPopup("ExportAsWav");
                // lock the rePlayer to avoid opening the systray menu
                Core::Lock();
            }
            else
            {
                delete m_export;
                m_export = nullptr;
            }
        }
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("ExportAsWav", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            float progress;
            uint32_t duration;
            auto songIndex = m_export->GetStatus(progress, duration);
            MusicID musicId = m_export->GetMusicId(songIndex);

            ImGui::BeginChild("Labels", ImVec2(320.0f, ImGui::GetFrameHeight() * 3 + ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("ID     : %08X%c", musicId.subsongId.value, musicId.databaseId == DatabaseID::kPlaylist ? 'p' : 'l');
            ImGui::Text("Title  : %s", musicId.GetTitle().c_str());
            ImGui::Text(musicId.GetSong()->NumArtistIds() > 1 ? "Artists: %s" : "Artist : %s", musicId.GetArtists().c_str());
            ImGui::Text("Replay : %s", musicId.GetSong()->GetType().GetReplay());
            ImGui::EndChild();
            {
                auto numSongs = m_export->NumSongs();
                char buf[32];
                sprintf(buf, "Song %d/%d", songIndex + 1, numSongs);
                ImGui::ProgressBar((songIndex + 1.0f) / numSongs, ImVec2(-FLT_MIN, 0), buf);
            }
            if (duration != 0xffFFffFF)
            {
                char buf[32];
                sprintf(buf, "%d:%02d", duration / 60, duration % 60);
                ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0), buf);
            }
            else
                ImGui::ProgressBar(1.0f, ImVec2(-FLT_MIN, 0), "Writing WAV");

            if (ImGui::Button("Cancel", ImVec2(-FLT_MIN, 0.0f)))
                m_export->Cancel();

            if (m_export->IsDone())
            {
                delete m_export;
                m_export = nullptr;
                ImGui::CloseCurrentPopup();
                Core::Unlock();
            }

            ImGui::EndPopup();
        }
    }

    void DatabaseSongsUI::SortSubsongs(bool isDirty)
    {
        auto* sortsSpecs = ImGui::TableGetSortSpecs();
        if (sortsSpecs && (sortsSpecs->SpecsDirty || isDirty) && m_entries.NumItems() > 1)
        {
            std::sort(m_entries.begin(), m_entries.end(), [this, sortsSpecs](auto& l, auto& r)
            {
                Song* lSong = m_db[l.id.songId];
                Song* rSong = m_db[r.id.songId];
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
                        delta = CompareStringMixed(m_db.GetArtists(l.id.songId).c_str(), m_db.GetArtists(r.id.songId).c_str());
                        break;
                    case kType:
                        delta = strcmp(lSong->GetType().GetExtension(), rSong->GetType().GetExtension());
                        break;
                    case kSize:
                        delta = int64_t(lSong->GetFileSize()) - int64_t(rSong->GetFileSize());
                        break;
                    case kDuration:
                        delta = int64_t(lSong->GetSubsongDurationCs(l.id.index) / 100) - int64_t(rSong->GetSubsongDurationCs(r.id.index) / 100);
                        break;
                    case kCRC:
                        delta = int64_t(lSong->GetFileCrc()) - int64_t(rSong->GetFileCrc());
                        break;
                    case kRating:
                        delta = int64_t(lSong->GetSubsongRating(l.id.index)) - int64_t(rSong->GetSubsongRating(r.id.index));
                        break;
                    case kDatabaseDate:
                        delta = int64_t(lSong->GetDatabaseDay()) - int64_t(rSong->GetDatabaseDay());
                        break;
                    }

                    if (delta)
                        return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                }
                return l.id.value < r.id.value;
            });

            sortsSpecs->SpecsDirty = false;
        }
    }

    void DatabaseSongsUI::SetRowBackground(int32_t rowIdx, Song* song, SubsongID subsongId, MusicID currentPlayingSong)
    {
        float backgroundRatio = m_subsongHighlights.Get(subsongId, 0.0f);
        if (currentPlayingSong.databaseId == m_databaseId && subsongId == currentPlayingSong.subsongId)
            Core::GetDeck().DisplayProgressBarInTable(backgroundRatio);
        else
        {
            if (song->IsInvalid())
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowIdx & 1 ? ImVec4(0.20f, 0.10f, 0.10f, 1.0f) : ImVec4(0.20f, 0.10f, 0.10f, 0.93f)));
            else if (song->IsUnavailable())
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowIdx & 1 ? ImVec4(0.20f, 0.20f, 0.00f, 1.0f) : ImVec4(0.20f, 0.20f, 0.00f, 0.93f)));
            else if (!song->IsSubsongPlayed(subsongId.index))
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowIdx & 1 ? ImVec4(0.25f, 0.25f, 0.25f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 0.93f)));
            if (backgroundRatio > 0.0f)
            {
                auto col32 = ImGui::GetCurrentTable()->RowBgColor[0];
                auto col = col32 == IM_COL32_DISABLE ? ImGui::GetStyleColorVec4(rowIdx & 1 ? ImGuiCol_TableRowBgAlt : ImGuiCol_TableRowBg) : ImGui::ColorConvertU32ToFloat4(col32);
                col.x = std::lerp(col.x, 1.0f, backgroundRatio);
                col.y = std::lerp(col.y, 1.0f, backgroundRatio);
                col.z = std::lerp(col.z, 1.0f, backgroundRatio);
                col.w = std::lerp(col.w, 1.0f, backgroundRatio);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(col));
            }
        }
    }

    void DatabaseSongsUI::Selection(int32_t rowIdx, MusicID musicId)
    {
        // Update the selection
        bool isSelected = m_entries[rowIdx].isSelected;
        if (ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastFirstRowHeight)))//tbd: using imgui_internal for row height
            isSelected = Select(rowIdx, musicId, isSelected);
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
                m_entries[rowIdx].isSelected = isSelected = true;
                m_numSelectedEntries++;
            }
            DragSelection();
            ImGui::EndDragDropSource();
        }
        // Context menu
        if (ImGui::BeginPopupContextItem("Song Popup"))
        {
            if (!isSelected)
            {
                m_entries[rowIdx].isSelected = isSelected = true;
                m_numSelectedEntries++;
            }
            SelectionContext(isSelected);
            ImGui::EndPopup();
        }
    }

    bool DatabaseSongsUI::Select(int32_t rowIdx, MusicID musicId, bool isSelected)
    {
        Core::GetSongEditor().OnSongSelected(musicId);

        if (ImGui::GetIO().KeyShift)
        {
            if (!ImGui::GetIO().KeyCtrl)
            {
                for (auto& entry : m_entries)
                    entry.isSelected = false;
            }
            auto lastSelectedIndex = m_entries.FindIf<int32_t>([this](auto& entry)
            {
                return entry.id == m_lastSelectedSubsong;
            });
            if (lastSelectedIndex < 0)
            {
                lastSelectedIndex = rowIdx;
                m_lastSelectedSubsong = musicId.subsongId;
            }
            if (lastSelectedIndex <= rowIdx) for (; lastSelectedIndex <= rowIdx; lastSelectedIndex++)
                m_entries[lastSelectedIndex].isSelected = true;
            else for (int32_t i = rowIdx; i <= lastSelectedIndex; i++)
                m_entries[i].isSelected = true;
            isSelected = true;
            m_numSelectedEntries = 0;
            for (auto& entry : m_entries)
            {
                if (entry.isSelected)
                    m_numSelectedEntries++;
            }
        }
        else
        {
            m_lastSelectedSubsong = musicId.subsongId;
            if (ImGui::GetIO().KeyCtrl)
            {
                m_entries[rowIdx].isSelected = !m_entries[rowIdx].isSelected;
                isSelected ? m_numSelectedEntries-- : m_numSelectedEntries++;
            }
            else
            {
                for (auto& entry : m_entries)
                    entry.isSelected = false;
                m_entries[rowIdx].isSelected = !isSelected;
                m_numSelectedEntries = isSelected ? 1 : 0;
            }
            isSelected = !isSelected;
        }

        if (ImGui::IsMouseDoubleClicked(0))
        {
            Core::GetDeck().PlaySolo(musicId);
        }
        return isSelected;
    }

    void DatabaseSongsUI::DragSelection()
    {
        Array<MusicID> subsongsList;
        for (auto& entry : m_entries)
        {
            if (entry.isSelected)
                subsongsList.Add(MusicID(entry.id, m_databaseId));
        }
        ImGui::SetDragDropPayload("DATABASE", subsongsList.begin(), subsongsList.Size());

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

    void DatabaseSongsUI::SelectionContext(bool isSelected)
    {
        if (ImGui::Selectable("Invert selection"))
        {
            for (auto& entry : m_entries)
                entry.isSelected = !entry.isSelected;
            isSelected = !isSelected;
            m_numSelectedEntries = m_entries.NumItems() - m_numSelectedEntries;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Add to playlist"))
        {
            for (auto& entry : m_entries)
            {
                if (entry.isSelected)
                    Core::Enqueue(MusicID(entry.id, m_databaseId));
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::BeginMenu("Add to artist"))
        {
            AddToArtist();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Tags"))
        {
            EditTags();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Discard"))
        {
            isSelected = Discard();
            ImGui::EndMenu();
        }
        if (ImGui::Selectable("Export as WAV"))
        {
            m_isExportAsWavTriggered = true;
            ImGui::CloseCurrentPopup();
        }
        OnSelectionContext();
    }

    void DatabaseSongsUI::AddToArtist()
    {
        // Build and sort the artist list
        Array<Artist*> artists(0ull, m_db.NumArtists());
        for (Artist* artist : m_db.Artists())
            artists.Add(artist);
        std::sort(artists.begin(), artists.end(), [this](auto l, auto r)
        {
            return _stricmp(l->GetHandle(), r->GetHandle()) < 0;
        });

        // Select
        ImGuiListClipper clipper;
        clipper.Begin(artists.NumItems<int32_t>() + 1);
        ArtistID selectedNewArtistId = ArtistID::Invalid;
        while (clipper.Step())
        {
            auto clipperDisplayStart = clipper.DisplayStart;
            if (clipperDisplayStart == 0)
            {
                ImGui::PushID(-1);
                if (ImGui::Selectable("-- New Artist --", false))
                {
                    auto artist = new ArtistSheet();
                    m_db.AddArtist(artist);
                    selectedNewArtistId = artist->id;
                    artist->handles.Add("-- New Artist (EDIT ME!!) --");
                }
                ImGui::PopID();
                clipperDisplayStart++;
            }

            for (int rowIdx = clipperDisplayStart - 1; rowIdx < clipper.DisplayEnd - 1; rowIdx++)
            {
                ImGui::PushID(static_cast<int>(artists[rowIdx]->GetId()));
                if (ImGui::Selectable(artists[rowIdx]->GetHandle(), false))
                    selectedNewArtistId = artists[rowIdx]->GetId();
                ImGui::PopID();
            }
        }

        // assign the artist
        if (selectedNewArtistId != ArtistID::Invalid)
        {
            auto* artist = m_db[selectedNewArtistId];
            Array<SongID> songs;
            for (auto& entry : m_entries)
            {
                if (entry.isSelected && !!songs.AddOnce(entry.id.songId).second)
                {
                    auto* song = m_db[entry.id];
                    if (song->ArtistIds().Find(selectedNewArtistId) == nullptr)
                    {
                        artist->Edit()->numSongs++;
                        OnAddingArtistToSong(song, selectedNewArtistId);
                        m_db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                    }
                }
            }
        }
    }

    void DatabaseSongsUI::EditTags()
    {
        Tag disabledTags = Tag::kNone;
        Tag enabledTags = Tag::kNone;
        for (auto& entry : m_entries)
        {
            if (entry.isSelected)
            {
                enabledTags.Raise(m_db[entry.id]->GetTags());
                disabledTags.Raise(Tag(~0ull).Lower(m_db[entry.id]->GetTags()));
            }
        }
        for (uint32_t i = 0; i < Tag::kNumTags; i++)
        {
            bool isChecked = enabledTags.IsEnabled(1ull << i);
            if (isChecked && disabledTags.IsEnabled(1ull << i))
            {
                ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
                bool b = false;
                if (ImGui::Checkbox(Tag::Name(i), &b))
                {
                    for (auto& entry : m_entries)
                    {
                        if (entry.isSelected)
                            m_db[entry.id]->Edit()->tags.Raise(1ull << i);
                    }
                    m_db.Raise(Database::Flag::kSaveSongs);
                }
                ImGui::PopItemFlag();
            }
            else
            {
                if (ImGui::Checkbox(Tag::Name(i), &isChecked))
                {
                    for (auto& entry : m_entries)
                    {
                        if (entry.isSelected)
                            m_db[entry.id]->Edit()->tags.Enable(1ull << i, isChecked);
                    }
                    m_db.Raise(Database::Flag::kSaveSongs);
                }
            }
        }
    }

    bool DatabaseSongsUI::Discard()
    {
        std::string discardLabel;
        for (auto& entry : m_entries)
        {
            if (entry.isSelected)
            {
                auto discardSong = m_db[entry.id];
                discardLabel += "[";
                discardLabel += discardSong->GetType().GetExtension();
                discardLabel += "] ";
                discardLabel += m_db.GetArtists(entry.id.songId);
                discardLabel += " - ";
                discardLabel += m_db.GetTitle(entry.id);
                discardLabel += "\n";
            }
        }
        if (ImGui::Selectable(discardLabel.c_str()))
        {
            for (auto& entry : m_entries)
            {
                if (entry.isSelected)
                {
                    m_deletedSubsongs.Add(entry.id);
                    entry.isSelected = false;
                }
            }
            m_numSelectedEntries = 0;
            ImGui::CloseCurrentPopup();
            return false;
        }
        return true;
    }
}
// namespace rePlayer