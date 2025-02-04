// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>
#include <Thread/Thread.h>

// rePlayer
#include <Database/Database.h>
#include <Database/SongEditor.h>
#include <Database/Types/Countries.h>
#include <Deck/Deck.h>
#include <RePlayer/Core.h>
#include <RePlayer/Export.h>

#include "DatabaseSongsUI.h"

// stl
#include <algorithm> // std::swap
#include <bit> // std::countr_zero
#include <chrono> // std::chrono

namespace rePlayer
{
    inline constexpr DatabaseSongsUI::FilterFlag operator|(DatabaseSongsUI::FilterFlag a, DatabaseSongsUI::FilterFlag b)
    {
        a = DatabaseSongsUI::FilterFlag(DatabaseSongsUI::FilterFlagType(a) | DatabaseSongsUI::FilterFlagType(b));
        return a;
    }

    inline constexpr DatabaseSongsUI::FilterFlag& operator^=(DatabaseSongsUI::FilterFlag& a, DatabaseSongsUI::FilterFlagType b)
    {
        a = DatabaseSongsUI::FilterFlag(DatabaseSongsUI::FilterFlagType(a) ^ b);
        return a;
    }

    inline constexpr bool operator&&(DatabaseSongsUI::FilterFlag a, DatabaseSongsUI::FilterFlag b)
    {
        return DatabaseSongsUI::FilterFlagType(a) & DatabaseSongsUI::FilterFlagType(b);
    }

    DatabaseSongsUI::DatabaseSongsUI(DatabaseID databaseId, Window& owner, bool isScrollingEnabled, uint16_t defaultHiddenColumns, const char* header)
        : m_db(Core::GetDatabase(databaseId))
        , m_owner(owner)
        , m_header(header)
        , m_databaseId(databaseId)
        , m_defaultHiddenColumns(defaultHiddenColumns)
        , m_isScrollingEnabled(isScrollingEnabled)
    {
        m_db.Register(this);
        m_filters.Add({ .flags = FilterFlag::kSongTitle | FilterFlag::kArtistHandle, .id = 0, .ui = new ImGuiTextFilter() });
        owner.RegisterSerializedCustomData(this, [](void* data, const char* line)
        {
            auto* This = reinterpret_cast<DatabaseSongsUI*>(data);
            if (strstr(line, This->m_header.c_str()))
            {
                line += This->m_header.size();
                uint32_t numFilters;
                if (sscanf_s(line, "FilterCount=%u", &numFilters) == 1)
                {
                    This->m_filters.Resize(numFilters);
                    for (uint32_t i = 1; i < numFilters; i++)
                        This->m_filters[i] = { .flags = FilterFlag::kSongTitle | FilterFlag::kArtistHandle, .id = i, .ui = new ImGuiTextFilter() };
                    return true;
                }
                else if (strstr(line, "Filter") == line)
                {
                    uint32_t filterIndex;
                    uint32_t flags;
                    if (sscanf_s(line, "Filter%u=%u", &filterIndex, &flags) == 2)
                    {
                        if (auto* c = strstr(line, "/"))
                        {
                            This->m_filters[filterIndex].flags = FilterFlag(flags);
                            strcpy_s(This->m_filters[filterIndex].ui->InputBuf, c + 1);
                            This->m_filters[filterIndex].ui->Build();
                            return true;
                        }
                    }
                }
            }
            return false;

        }, [](void* data, ImGuiTextBuffer* buf)
        {
            auto* This = reinterpret_cast<DatabaseSongsUI*>(data);
            auto numItems = This->m_filters.NumItems();
            buf->appendf("%sFilterCount=%u\n", This->m_header.c_str(), numItems);
            for (uint32_t i = 0; i < numItems; i++)
                buf->appendf("%sFilter%u=%u/%s\n", This->m_header.c_str(), i, FilterFlagType(This->m_filters[i].flags), This->m_filters[i].ui->InputBuf);
        });
    }

    DatabaseSongsUI::~DatabaseSongsUI()
    {
        if (m_export)
        {
            m_export->Cancel();
            while (!m_export->IsDone())
                thread::Sleep(1);
            delete m_export;
        }
        for (auto& filter : m_filters)
            delete filter.ui;
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
        bool isDirty = m_dbSongsRevision != m_db.SongsRevision();
        m_dbSongsRevision = m_db.SongsRevision();

        DisplaySongsFilter(isDirty);
        DisplaySongsTable(isDirty);
        DisplayExportAsWav();
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

        RemoveFromArtist();
    }

    Array<DatabaseSongsUI::SubsongEntry> DatabaseSongsUI::GatherEntries() const
    {
        Array<SubsongEntry> entries;
        for (auto* song : m_db.Songs())
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
        }
        return entries;
    }

    void DatabaseSongsUI::OnSelectionContext()
    {}

    bool DatabaseSongsUI::AddToPlaylistUI()
    {
        bool isSelected = ImGui::Selectable("Add to playlist");
        if (isSelected)
        {
            for (auto& entry : m_entries)
            {
                if (entry.IsSelected())
                    Core::Enqueue(MusicID(entry, m_databaseId));
            }
        }
        return isSelected;
    }

    void DatabaseSongsUI::DisplaySongsFilter(bool& isDirty)
    {
        DisplaySongsFilterUI(isDirty);
        if (isDirty)
            FilterSongs();
    }

    void DatabaseSongsUI::DisplaySongsTable(bool& isDirty)
    {
        float trackingPos = FLT_MAX;
        // Create a child to be able to detect the focus and do the Ctrl-A
        std::string childId = m_header + "Child";
        if (ImGui::BeginChild(childId.c_str(), ImVec2(0.0f, 0.0f), m_isScrollingEnabled ? 0 : ImGuiChildFlags_AutoResizeY))
        {
            const ImGuiTableFlags flags = (m_isScrollingEnabled ? ImGuiTableFlags_ScrollY : ImGuiTableFlags_None)
                | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
                | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Hideable;

            if (ImGui::BeginTable("songs", kNumIDs, flags))
            {
                auto getHiddenFlag = [this](uint32_t flag)
                {
                    return (m_defaultHiddenColumns & (1 << flag)) ? ImGuiTableColumnFlags_DefaultHide : 0;
                };

                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch | getHiddenFlag(kTitle), 0.0f, kTitle);
                ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthStretch | getHiddenFlag(kArtist), 0.0f, kArtist);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed | getHiddenFlag(kType), 0.0f, kType);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | getHiddenFlag(kSize), 0.0f, kSize);
                ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed | getHiddenFlag(kDuration), 0.0f, kDuration);
                ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthFixed | getHiddenFlag(kYear), 0.0f, kYear);
                ImGui::TableSetupColumn("CRC", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | getHiddenFlag(kCRC), 0.0f, kCRC);
                ImGui::TableSetupColumn(">|", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | getHiddenFlag(kState), 0.0f, kState);
                ImGui::TableSetupColumn("[%]", ImGuiTableColumnFlags_WidthFixed | getHiddenFlag(kRating), 0.0f, kRating);
                ImGui::TableSetupColumn("Added", ImGuiTableColumnFlags_WidthFixed | getHiddenFlag(kDatabaseDate), 0.0f, kDatabaseDate);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch | getHiddenFlag(kSource), 0.0f, kSource);
                ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_WidthStretch | getHiddenFlag(kReplay), 0.0f, kReplay);
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
                        auto musicId = MusicID(m_entries[rowIdx], m_databaseId);
                        Song* song = m_db[musicId.subsongId];

                        ImGui::TableNextColumn();
                        ImGui::PushID(&musicId.subsongId, &musicId.subsongId.external);

                        UpdateRowBackground(rowIdx, song, musicId.subsongId, currentPlayingSong);
                        UpdateSelection(rowIdx, musicId);
                        ImGui::SameLine(0.0f, 0.0f);//no spacing
                        ImGui::TextUnformatted(m_db.GetTitle(musicId.subsongId).c_str());
                        if (ImGui::IsItemHovered())
                            musicId.SongTooltip();
                        ImGui::TableNextColumn();
                        musicId.DisplayArtists();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(song->GetType().GetExtension());
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
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", SourceID::sourceNames[song->GetSourceId(0).sourceId]);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", song->GetType().GetReplay());
                        ImGui::PopID();
                    }
                }

                // build the tracking position and set it if the scrolling is enabled for the table
                if (m_trackedSubsongId.IsValid())
                {
                    auto trackedSubsongIndex = m_entries.FindIf<int32_t>([this](auto& entry)
                    {
                        return entry == m_trackedSubsongId;
                    });
                    if (trackedSubsongIndex >= 0)
                    {
                        trackingPos = clipper.StartPosY + clipper.ItemsHeight * trackedSubsongIndex;
                        if (m_isScrollingEnabled)
                            ImGui::SetScrollFromPosY(trackingPos - ImGui::GetWindowPos().y);
                    }

                    if (m_isScrollingEnabled)
                        m_trackedSubsongId = {};
                }

                ImGui::EndTable();
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
            {
                m_numSelectedEntries = m_entries.NumItems();
                for (auto& entry : m_entries)
                    entry.Select(true);
            }
        }
        ImGui::EndChild();

        // Track now if the scrolling is not enabled in the table
        if (m_trackedSubsongId.IsValid())
        {
            if (trackingPos != FLT_MAX)
                ImGui::SetScrollFromPosY(trackingPos - ImGui::GetWindowPos().y);
            // check this to fix the bug of not tracked when the window is appearing
            if (!ImGui::IsWindowAppearing())
                m_trackedSubsongId = {};
        }
    }

    void DatabaseSongsUI::DisplaySongsFilterUI(bool& isDirty)
    {
        ImGui::PushID("FilterBarsUI");

        const float buttonSize = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();

        struct Op
        {
            bool isAdding = false;
            bool isRemoving = false;
            bool isSwappingUp = false;
            bool isSwappingDown = false;
            int index;
        } op;
        for (int filterIndex = 0, lastIndex = m_filters.NumItems<int>() - 1; filterIndex <= lastIndex; filterIndex++)
        {
            ImGui::PushID(m_filters[filterIndex].id);

            auto filterFlags = m_filters[filterIndex].flags;

            static const char* filterName[] = {
                "Title",
                "Artist",
                "Subsong",
                "Name",
                "Alias",
                "Country",
                "Group",
                "Tag",
                "Type",
                "Replay",
                "Source"
            };
            static_assert(_countof(filterName) == kNumFilterFlags);

            std::string filterIdPreview;
            for (auto flags = FilterFlagType(filterFlags); flags;)
            {
                auto idx = std::countr_zero(flags);
                if (!filterIdPreview.empty())
                    filterIdPreview += '+';
                filterIdPreview += filterName[idx];
                flags &= ~(1 << idx);
            }

            ImGui::SetNextItemWidth(128.0f);
            if (ImGui::BeginCombo("##FilterIDs", filterIdPreview.c_str()))
            {
                for (uint32_t i = 0; i < kNumFilterFlags; i++)
                {
                    bool isEnabled = FilterFlagType(filterFlags) & (FilterFlagType(1) << i);
                    if (ImGui::Checkbox(filterName[i], &isEnabled))
                        filterFlags ^= (FilterFlagType(1) << i);
                }

                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip(filterIdPreview.c_str());
            if (filterFlags == FilterFlag::kNone)
                filterFlags = FilterFlag::kSongTitle;
            if (filterFlags != m_filters[filterIndex].flags)
            {
                isDirty = true;
                m_filters[filterIndex].flags = filterFlags;
            }

            ImGui::SameLine();
            static constexpr uint32_t kNumButtons = 3;
            isDirty |= m_filters[filterIndex].ui->Draw("##filter", -(buttonSize + style.ItemInnerSpacing.x) * kNumButtons);

            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(filterIndex == 0);
            if (ImGui::Button("^", ImVec2(buttonSize, 0.0f)))
                op = { .isSwappingUp = true, .index = filterIndex };
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Move filter up");
            ImGui::EndDisabled();

            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(lastIndex == (kMaxFilters - 1));
            if (ImGui::Button(filterIndex == lastIndex ? "+" : "v", ImVec2(buttonSize, 0.0f)))
                op = { .isAdding = filterIndex == lastIndex, .isSwappingDown = filterIndex != lastIndex, .index = filterIndex };
            if (ImGui::IsItemHovered())
                ImGui::Tooltip(filterIndex == lastIndex ? "Add filter" : "Move filter down");
            ImGui::EndDisabled();

            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(lastIndex == 0);
            if (ImGui::Button("X", ImVec2(buttonSize, 0.0f)))
                op = { .isRemoving = true, .index = filterIndex };
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Remove filter");
            ImGui::EndDisabled();

            ImGui::PopID();
        }
        if (op.isAdding)
        {
            m_filters.Add({ .flags = FilterFlag::kSongTitle, .id = m_filterLostIds.IsEmpty() ? m_filters.NumItems() : m_filterLostIds.Pop(), .ui = new ImGuiTextFilter() });
        }
        else if (op.isRemoving)
        {
            delete m_filters[op.index].ui;
            m_filters.RemoveAt(op.index);
            isDirty = true;
        }
        else if (op.isSwappingDown)
        {
            std::swap(m_filters[op.index], m_filters[op.index + 1]);
            isDirty = true;
        }
        else if (op.isSwappingUp)
        {
            std::swap(m_filters[op.index - 1], m_filters[op.index]);
            isDirty = true;
        }

        ImGui::PopID();
    }

    void DatabaseSongsUI::DisplayExportAsWav()
    {
        if (m_isExportAsWavTriggered)
        {
            m_isExportAsWavTriggered = false;
            m_export = new Export();
            for (auto& entry : m_entries)
            {
                if (entry.IsSelected())
                    m_export->Enqueue({ entry, m_databaseId });
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

            ImGui::BeginChild("Labels", ImVec2(320.0f, ImGui::GetFrameHeight() * 3 + ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None, ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("ID     : %016llX%c", musicId.subsongId.Value(), musicId.databaseId == DatabaseID::kPlaylist ? 'p' : 'l');
            ImGui::Text("Title  : %s", musicId.GetTitle().c_str());
            ImGui::Text(musicId.GetSong()->NumArtistIds() > 1 ? "Artists: %s" : "Artist : %s", musicId.GetArtists().c_str());
            ImGui::Text("Replay : %s", musicId.GetSong()->GetType().GetReplay());
            ImGui::EndChild();
            {
                auto numSongs = m_export->NumSongs();
                char buf[32];
                sprintf(buf, "Song %u/%u", songIndex + 1, numSongs);
                ImGui::ProgressBar((songIndex + 1.0f) / numSongs, ImVec2(-FLT_MIN, 0), buf);
            }
            if (duration != 0xffFFffFF)
            {
                char buf[32];
                sprintf(buf, "%u:%02u", uint32_t(duration / 60), uint32_t(duration % 60));
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

    void DatabaseSongsUI::FilterSongs()
    {
        // prepare selection
        m_numSelectedEntries = 0;
        auto oldEntries = std::move(m_entries);
        for (int64_t oldEntryIndex = 0, numOldEntries = oldEntries.NumItems<int64_t>(); oldEntryIndex < numOldEntries;)
        {
            if (!oldEntries[oldEntryIndex].IsSelected())
            {
                oldEntries.RemoveAtFast(oldEntryIndex);
                --numOldEntries;
            }
            else
                ++oldEntryIndex;
        }
        // filter entries
        Array<SubsongEntry> entries = GatherEntries();
        std::string textToFilter;
        for (auto& filter : m_filters)
        {
            if (!filter.ui->IsActive())
                continue;
            auto flags = filter.flags;

            auto updateTextToFilter = [&textToFilter](const char* text)
            {
                if (!textToFilter.empty())
                    textToFilter += '\n';
                textToFilter += text;
            };

            for (int64_t entryIndex = 0, numEntries = entries.NumItems<int64_t>(); entryIndex < numEntries; ++entryIndex)
            {
                auto* song = m_db[entries[entryIndex]];
                textToFilter.clear();
                if (flags && FilterFlag::kSongTitle)
                    updateTextToFilter(song->GetName());
                if (flags && FilterFlag::kArtistHandle)
                    for (auto artistId : song->ArtistIds())
                        updateTextToFilter(m_db[artistId]->GetHandle());
                if (flags && FilterFlag::kSubongTitle)
                    updateTextToFilter(song->GetSubsongName(entries[entryIndex].index));
                if (flags && FilterFlag::kArtistName)
                    for (auto artistId : song->ArtistIds())
                        updateTextToFilter(m_db[artistId]->GetRealName());
                if (flags && FilterFlag::kArtistAlias)
                    for (auto artistId : song->ArtistIds())
                        for (uint16_t i = 1, numHandles = m_db[artistId]->NumHandles(); i < numHandles; i++)
                            updateTextToFilter(m_db[artistId]->GetHandle(i));
                if (flags && FilterFlag::kArtistCountry)
                    for (auto artistId : song->ArtistIds())
                        for (uint16_t i = 0, numCountries = m_db[artistId]->NumCountries(); i < numCountries; i++)
                            updateTextToFilter(Countries::GetName(m_db[artistId]->GetCountry(i)));
                if (flags && FilterFlag::kArtistGroup)
                    for (auto artistId : song->ArtistIds())
                        for (uint16_t i = 0, numGroups = m_db[artistId]->NumGroups(); i < numGroups; i++)
                            updateTextToFilter(m_db[artistId]->GetGroup(i));
                if (flags && FilterFlag::kSongTag)
                    updateTextToFilter(song->GetTags().ToString().c_str());
                if (flags && FilterFlag::kSongType)
                    updateTextToFilter(song->GetType().GetExtension());
                if (flags && FilterFlag::kReplay)
                    updateTextToFilter(song->GetType().GetReplay());
                if (flags && FilterFlag::kSource)
                    for (auto sourceId : song->SourceIds())
                        updateTextToFilter(SourceID::sourceNames[sourceId.sourceId]);

                if (!filter.ui->PassFilter(textToFilter.c_str(), textToFilter.c_str() + textToFilter.size()))
                {
                    entries.RemoveAtFast(entryIndex--);
                    --numEntries;
                }
            }
        }
        // assign the selection
        for (int64_t entryIndex = 0, numEntries = entries.NumItems<int64_t>(); entryIndex < numEntries; ++entryIndex)
        {
            for (int64_t oldEntryIndex = 0, numOldEntries = oldEntries.NumItems<int64_t>(); oldEntryIndex < numOldEntries; ++oldEntryIndex)
            {
                if (oldEntries[oldEntryIndex] == entries[entryIndex])
                {
                    entries[entryIndex].Select(true);
                    oldEntries.RemoveAtFast(oldEntryIndex);
                    m_numSelectedEntries++;
                    break;
                }
            }
        }
        m_entries = std::move(entries.Refit());
    }

    void DatabaseSongsUI::SortSubsongs(bool isDirty)
    {
        auto* sortsSpecs = ImGui::TableGetSortSpecs();
        if (sortsSpecs && (sortsSpecs->SpecsDirty || isDirty) && m_entries.NumItems() > 1)
        {
            std::sort(m_entries.begin(), m_entries.end(), [this, sortsSpecs](auto& l, auto& r)
            {
                Song* lSong = m_db[l.songId];
                Song* rSong = m_db[r.songId];
                for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                {
                    auto& sortSpec = sortsSpecs->Specs[i];
                    int64_t delta = 0;
                    switch (sortSpec.ColumnUserID)
                    {
                    case kTitle:
                        delta = CompareStringMixed(lSong->GetName(), rSong->GetName());
                        break;
                    case kArtist:
                        delta = CompareStringMixed(m_db.GetArtists(l.songId).c_str(), m_db.GetArtists(r.songId).c_str());
                        break;
                    case kType:
                        delta = strcmp(lSong->GetType().GetExtension(), rSong->GetType().GetExtension());
                        break;
                    case kSize:
                        delta = int64_t(lSong->GetFileSize()) - int64_t(rSong->GetFileSize());
                        break;
                    case kDuration:
                        delta = int64_t(lSong->GetSubsongDurationCs(l.index) / 100) - int64_t(rSong->GetSubsongDurationCs(r.index) / 100);
                        break;
                    case kCRC:
                        delta = int64_t(lSong->GetFileCrc()) - int64_t(rSong->GetFileCrc());
                        break;
                    case kRating:
                        delta = int64_t(lSong->GetSubsongRating(l.index)) - int64_t(rSong->GetSubsongRating(r.index));
                        break;
                    case kDatabaseDate:
                        delta = int64_t(lSong->GetDatabaseDay()) - int64_t(rSong->GetDatabaseDay());
                        break;
                    case kSource:
                        delta = strcmp(SourceID::sourceNames[lSong->GetSourceId(0).sourceId], SourceID::sourceNames[rSong->GetSourceId(0).sourceId]);
                        break;
                    case kReplay:
                        delta = strcmp(lSong->GetType().GetReplay(), rSong->GetType().GetReplay());
                        break;
                    }

                    if (delta)
                        return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                }
                return l < r;
            });

            sortsSpecs->SpecsDirty = false;
        }
    }

    void DatabaseSongsUI::UpdateRowBackground(int32_t rowIdx, Song* song, SubsongID subsongId, MusicID currentPlayingSong)
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

    void DatabaseSongsUI::UpdateSelection(int32_t rowIdx, MusicID musicId)
    {
        // Update the selection
        bool isSelected = m_entries[rowIdx].IsSelected();
        if (ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastTopHeadersRowHeight)))//tbd: using imgui_internal for row height
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
                m_entries[rowIdx].Select(isSelected = true);
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
                m_entries[rowIdx].Select(isSelected = true);
                m_numSelectedEntries++;
            }
            UpdateSelectionContext(isSelected);
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
                    entry.Select(false);
            }
            auto lastSelectedIndex = m_entries.FindIf<int32_t>([this](auto& entry)
            {
                return entry == m_lastSelectedSubsong;
            });
            if (lastSelectedIndex < 0)
            {
                lastSelectedIndex = rowIdx;
                m_lastSelectedSubsong = musicId.subsongId;
            }
            if (lastSelectedIndex <= rowIdx) for (; lastSelectedIndex <= rowIdx; lastSelectedIndex++)
                m_entries[lastSelectedIndex].Select(true);
            else for (int32_t i = rowIdx; i <= lastSelectedIndex; i++)
                m_entries[i].Select(true);
            isSelected = true;
            m_numSelectedEntries = 0;
            for (auto& entry : m_entries)
            {
                if (entry.IsSelected())
                    m_numSelectedEntries++;
            }
        }
        else
        {
            m_lastSelectedSubsong = musicId.subsongId;
            if (ImGui::GetIO().KeyCtrl)
            {
                m_entries[rowIdx].Select(!m_entries[rowIdx].IsSelected());
                isSelected ? m_numSelectedEntries-- : m_numSelectedEntries++;
            }
            else
            {
                for (auto& entry : m_entries)
                    entry.Select(false);
                m_entries[rowIdx].Select(!isSelected);
                m_numSelectedEntries = isSelected ? 0 : 1;
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
            if (entry.IsSelected())
                subsongsList.Add(MusicID(entry, m_databaseId));
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

    void DatabaseSongsUI::UpdateSelectionContext(bool isSelected)
    {
        if (ImGui::Selectable("Invert selection"))
        {
            for (auto& entry : m_entries)
                entry.Select(!entry.IsSelected());
            isSelected = !isSelected;
            m_numSelectedEntries = m_entries.NumItems() - m_numSelectedEntries;
            ImGui::CloseCurrentPopup();
        }
        if (AddToPlaylistUI())
            ImGui::CloseCurrentPopup();
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
        if (ImGui::BeginMenu("Add to artist"))
        {
            AddToArtist();
            ImGui::EndMenu();
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
        if (ImGui::BeginMenu("Remove from artist"))
        {
            RemoveFromArtistUI();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Tags"))
        {
            EditTags();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
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
        // Advanced commands (hidden until shift is pressed)
        if (ImGui::GetIO().KeyShift)
        {
            if (ImGui::BeginMenu("Reset replay"))
            {
                ResetReplay();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Reset state"))
            {
                ResetSubsongState();
                ImGui::EndMenu();
            }
        }
    }

    void DatabaseSongsUI::AddToArtist()
    {
        // Build and sort the artist list
        Array<Artist*> artists(size_t(0), m_db.NumArtists());
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
                if (entry.IsSelected() && !!songs.AddOnce(entry.songId).second)
                    m_db.AddArtistToSong(m_db[entry], artist);
            }
        }
    }

    void DatabaseSongsUI::RemoveFromArtistUI()
    {
        // build and sort the artist to remove list
        Array<ArtistID> artists;
        for (auto& entry : m_entries)
        {
            if (entry.IsSelected())
            {
                auto* song = m_db[entry];
                for (uint16_t i = 0, e = song->NumArtistIds(); i < e; ++i)
                    artists.AddOnce(song->GetArtistId(i));
            }
        }
        if (artists.IsNotEmpty())
        {
            // sort
            std::sort(artists.begin(), artists.end(), [this](auto l, auto r)
            {
                return _stricmp(m_db[l]->GetHandle(), m_db[r]->GetHandle()) < 0;
            });
            // select
            ImGuiListClipper clipper;
            clipper.Begin(artists.NumItems<int32_t>());
            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    ImGui::PushID(static_cast<int>(artists[rowIdx]));
                    auto* artist = m_db[artists[rowIdx]];
                    if (ImGui::Selectable(artist->GetHandle(), false))
                        m_artistToRemove = artists[rowIdx];
                    ImGui::PopID();
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
            if (entry.IsSelected())
            {
                enabledTags.Raise(m_db[entry]->GetTags());
                disabledTags.Raise(Tag(~0ull).Lower(m_db[entry]->GetTags()));
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
                        if (entry.IsSelected())
                            m_db[entry]->Edit()->tags.Raise(1ull << i);
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
                        if (entry.IsSelected())
                            m_db[entry]->Edit()->tags.Enable(1ull << i, isChecked);
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
            if (entry.IsSelected())
            {
                auto discardSong = m_db[entry];
                discardLabel += "[";
                discardLabel += discardSong->GetType().GetExtension();
                discardLabel += "] ";
                discardLabel += m_db.GetArtists(entry.songId);
                discardLabel += " - ";
                discardLabel += m_db.GetTitle(entry);
                discardLabel += "\n";
            }
        }
        auto pos = ImGui::GetCursorPos();
        if (ImGui::Selectable("##DiscardList", false, 0, ImGui::CalcTextSize(discardLabel.c_str(), discardLabel.c_str() + discardLabel.size())))
        {
            for (auto& entry : m_entries)
            {
                if (entry.IsSelected())
                {
                    m_db.DeleteSubsong(entry);
                    entry.Select(false);
                }
            }
            m_numSelectedEntries = 0;
            ImGui::CloseCurrentPopup();
            return false;
        }
        ImGui::SetCursorPos(pos);
        ImGui::TextUnformatted(discardLabel.c_str(), discardLabel.c_str() + discardLabel.size());
        return true;
    }

    void DatabaseSongsUI::ResetReplay()
    {
        for (uint32_t i = 0; i < uint32_t(eReplay::Count); i++)
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
            if (ImGui::BeginMenu(MediaType::replayNames[i]))
            {
                std::string resetLabel;
                for (auto& entry : m_entries)
                {
                    if (entry.IsSelected())
                    {
                        auto discardSong = m_db[entry];
                        resetLabel += "[";
                        resetLabel += discardSong->GetType().GetExtension();
                        resetLabel += "] ";
                        resetLabel += m_db.GetArtists(entry.songId);
                        resetLabel += " - ";
                        resetLabel += m_db.GetTitle(entry);
                        resetLabel += "\n";
                    }
                }
                auto pos = ImGui::GetCursorPos();
                if (ImGui::Selectable("##ResetList", false, 0, ImGui::CalcTextSize(resetLabel.c_str(), resetLabel.c_str() + resetLabel.size())))
                {
                    for (auto& entry : m_entries)
                    {
                        if (entry.IsSelected())
                        {
                            auto* song = m_db[entry]->Edit();
                            if (song->type.replay != eReplay(i))
                            {
                                song->type.replay = eReplay(i);
                                for (auto& subsong : song->subsongs)
                                    subsong.isPlayed = false;
                            }
                        }
                    }
                    m_db.Raise(Database::Flag::kSaveSongs);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetCursorPos(pos);
                ImGui::TextUnformatted(resetLabel.c_str(), resetLabel.c_str() + resetLabel.size());
                ImGui::EndMenu();
            }
        }
    }

    void DatabaseSongsUI::ResetSubsongState()
    {
        for (uint32_t i = 0; i < kNumSubsongStates; i++)
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
            static const char* const subsongStates[] = { "Undefined", "Stop", "Fade Out", "Loop Once" };
            if (ImGui::BeginMenu(subsongStates[i]))
            {
                std::string resetLabel;
                for (auto& entry : m_entries)
                {
                    if (entry.IsSelected())
                    {
                        auto discardSong = m_db[entry];
                        resetLabel += "[";
                        resetLabel += discardSong->GetType().GetExtension();
                        resetLabel += "] ";
                        resetLabel += m_db.GetArtists(entry.songId);
                        resetLabel += " - ";
                        resetLabel += m_db.GetTitle(entry);
                        resetLabel += "\n";
                    }
                }
                auto pos = ImGui::GetCursorPos();
                if (ImGui::Selectable("##ResetList", false, 0, ImGui::CalcTextSize(resetLabel.c_str(), resetLabel.c_str() + resetLabel.size())))
                {
                    for (auto& entry : m_entries)
                    {
                        if (entry.IsSelected())
                        {
                            auto* song = m_db[entry]->Edit();
                            song->subsongs[entry.index].state = SubsongState(i);
                        }
                    }
                    m_db.Raise(Database::Flag::kSaveSongs);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetCursorPos(pos);
                ImGui::TextUnformatted(resetLabel.c_str(), resetLabel.c_str() + resetLabel.size());
                ImGui::EndMenu();
            }
        }
    }

    void DatabaseSongsUI::RemoveFromArtist()
    {
        auto artistToRemove = m_artistToRemove;
        if (artistToRemove == ArtistID::Invalid)
            return;

        auto* artist = m_db[artistToRemove]->Edit();
        for (auto& entry : m_entries)
        {
            if (entry.IsSelected())
            {
                auto* song = m_db[entry];
                for (uint16_t i = 0, e = song->NumArtistIds(); i < e; i++)
                {
                    if (song->GetArtistId(i) == artistToRemove)
                    {
                        auto* songSheet = song->Edit();

                        artist->numSongs--;

                        auto oldFilename = m_db.GetFullpath(song);
                        songSheet->artistIds.Remove(artist->id);
                        if (!oldFilename.empty())
                            m_db.Move(oldFilename, song, "RemoveFromArtist");
                        break;
                    }
                }
            }
        }

        if (artist->numSongs == 0)
        {
            m_db.RemoveArtist(artistToRemove);
            Log::Message("RemoveFromArtist: Artist \"%s\" removed\n", artist->handles[0].String().c_str());
        }

        m_db.Raise(Database::Flag::kSaveArtists | Database::Flag::kSaveSongs);
        m_artistToRemove = ArtistID::Invalid;
    }
}
// namespace rePlayer