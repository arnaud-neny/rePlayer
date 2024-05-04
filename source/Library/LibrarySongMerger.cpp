#include "LibrarySongMerger.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>
#include <ImGui.h>

#include <Database/Database.h>
#include <Database/SongEditor.h>
#include <Deck/Deck.h>
#include <Deck/Player.h>
#include <RePlayer/Core.h>

#include <algorithm>

namespace rePlayer
{
    void Library::SongsUI::SongMerger::MenuItem(SongsUI& songs)
    {
        if (++m_frame != ImGui::GetFrameCount())
        {
            m_frame = ImGui::GetFrameCount();
            m_entries.Clear();
            m_lastSelectedEntryIndex = -1;
            m_numMergedEntries = 0;
            m_lastCheckedEntry = 0;
            for (auto& entry : songs.m_entries)
            {
                if (entry.isSelected && m_entries.Find(entry.id.songId) == nullptr)
                    m_entries.Add({ songs.m_db[entry.id] });
            }
        }

        ImGui::BeginDisabled(m_entries.NumItems() < 2);
        if (ImGui::Selectable("Merge songs"))
        {
            m_isStarted = true;

            std::sort(m_entries.begin(), m_entries.end(), [](auto& l, auto& r)
            {
                auto lFileSize = l.song->GetFileSize();
                auto rFileSize = r.song->GetFileSize();
                if (lFileSize != rFileSize)
                    return (uint64_t(lFileSize) - 1) < (uint64_t(rFileSize) - 1);

                if (lFileSize == 0)
                    return _stricmp(l.song->GetName(), r.song->GetName()) < 0;

                auto lFileCrc = l.song->GetFileCrc();
                auto rFileCrc = r.song->GetFileCrc();
                if (lFileCrc != rFileCrc)
                    return lFileCrc < rFileCrc;

                static int32_t sourceOrder[] = {
                    65536,  // AmigaMusicPreservationSourceID
                    65537,  // TheModArchiveSourceID
                    65538,  // ModlandSourceID
                    -2, // FileImportID
                    0,  // HighVoltageSIDCollectionID
                    1,  // SNDHID
                    2,  // AtariSAPMusicArchiveID
                    3,  // ZXArtID
                    4,  // VGMRips
                    -1  // UrlImportID
                };
                static_assert(_countof(sourceOrder) == SourceID::NumSourceIDs);

                auto lSourceId = sourceOrder[l.song->GetSourceId(0).sourceId];
                auto rSourceId = sourceOrder[r.song->GetSourceId(0).sourceId];
                return lSourceId < rSourceId;
            });

            for (uint32_t i = 1, e = m_entries.NumItems(); i < e; i++)
            {
                if (m_entries[i].song->GetFileSize() == 0 || m_entries[i - 1].song->GetFileSize() != m_entries[i].song->GetFileSize())
                    continue;
                if (m_entries[i - 1].song->GetFileCrc() != m_entries[i].song->GetFileCrc())
                    continue;

                if (m_entries[i - 1].parentId == SongID::Invalid)
                {
                    m_numMergedEntries++;
                    m_entries[i - 1].isRoot = true;
                    m_entries[i - 1].canMerge = true;
                    m_entries[i].parentId = m_entries[i - 1].song->GetId();
                }
                else
                    m_entries[i].parentId = m_entries[i - 1].parentId;
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
    }

    void Library::SongsUI::SongMerger::Update(SongsUI& songs)
    {
        if (m_isStarted)
        {
            ImGui::OpenPopup("Merge songs");
            m_isStarted = false;
            m_isOpened = true;
            m_isRenaming = false;
        }

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(0.0f, 256.f), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Merge songs", &m_isOpened, ImGuiWindowFlags_NoScrollbar))
        {
            int32_t unmergedEntryIndex = -1;
            int32_t droppedEntryIndex = -1;

            if (ImGui::BeginTable("Merge", 8, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, -ImGui::GetFrameHeightWithSpacing()))) // -GetFrameHeightWithSpacing to display the merge button
            {
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_None, 0.0f, 0);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_None, 0.0f, 1);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoResize, 0.0f, 2);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoResize, 0.0f, 3);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoResize, 0.0f, 4);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoResize, 0.0f, 5);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoResize, 0.0f, 6);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoResize, 0.0f, 7);

                ImGuiListClipper clipper;
                clipper.Begin(m_entries.NumItems<int>());
                while (clipper.Step())
                {
                    bool isIndented = m_entries[clipper.DisplayStart].parentId != SongID::Invalid;
                    for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                    {
                        ImGui::PushID(rowIdx);

                        auto* song = m_entries[rowIdx].song.Get();

                        // title + check box + child
                        ImGui::TableNextColumn();
                        ImGui::AlignTextToFramePadding(); // to avoid different height per lines between text only and widgets (check box)
                        if (ImGui::Selectable("##selected", m_entries[rowIdx].isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_DontClosePopups))
                        {
                            if (ImGui::IsMouseDoubleClicked(0))
                            {
                                if (ImGui::GetIO().KeyCtrl)
                                {
                                    for (uint16_t i = 0; i <= song->GetLastSubsongIndex(); i++)
                                    {
                                        if (!song->IsSubsongDiscarded(i))
                                        {
                                            Core::GetDeck().PlaySolo(songs.GetLibrary().LoadSong({SubsongID(song->GetId(), i), DatabaseID::kLibrary}));
                                            break;
                                        }
                                    }
                                }
                                else
                                    unmergedEntryIndex = rowIdx;
                            }
                            else if (ImGui::GetIO().KeyShift)
                            {
                                if (m_lastSelectedEntryIndex < 0)
                                    m_lastSelectedEntryIndex = rowIdx;
                                if (!ImGui::GetIO().KeyCtrl)
                                {
                                    for (auto& entry : m_entries)
                                        entry.isSelected = false;
                                }
                                auto currentEntryIndex = rowIdx;
                                auto lastEntryIndex = m_lastSelectedEntryIndex;
                                if (currentEntryIndex > lastEntryIndex)
                                    std::swap(currentEntryIndex, lastEntryIndex);
                                for (;currentEntryIndex <= lastEntryIndex; currentEntryIndex++)
                                    m_entries[currentEntryIndex].isSelected = true;
                            }
                            else
                            {
                                m_lastSelectedEntryIndex = rowIdx;
                                if (ImGui::GetIO().KeyCtrl)
                                {
                                    m_entries[rowIdx].isSelected = !m_entries[rowIdx].isSelected;
                                }
                                else
                                {
                                    auto isSelected = m_entries[rowIdx].isSelected;
                                    for (auto& entry : m_entries)
                                        entry.isSelected = false;
                                    m_entries[rowIdx].isSelected = !isSelected;
                                }
                            }
                        }
                        if (ImGui::IsItemHovered())
                        {
                            for (uint16_t i = 0; i <= song->GetLastSubsongIndex(); i++)
                            {
                                if (!song->IsSubsongDiscarded(i))
                                {
                                    MusicID({ song->GetId(), i }, DatabaseID::kLibrary).SongTooltip();
                                    break;
                                }
                            }
                        }
                        if (ImGui::BeginDragDropSource())
                        {
                            m_entries[rowIdx].isSelected = true;
                            ImGui::SetDragDropPayload("MERGESONGS", nullptr, 0);
                            for (uint32_t i = 0; i < m_entries.NumItems(); i++)
                            {
                                if (m_entries[i].isSelected)
                                    ImGui::TextUnformatted(m_entries[i].song->GetName());
                            }
                            ImGui::EndDragDropSource();
                        }
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (m_entries[rowIdx].isSelected)
                                ImGui::PushStyleColor(ImGuiCol_DragDropTarget, 0xff0000ff);
                            if (ImGui::AcceptDragDropPayload("MERGESONGS", 0))
                            {
                                if (!m_entries[rowIdx].isSelected)
                                {
                                    droppedEntryIndex = rowIdx;
                                    m_lastSelectedEntryIndex = rowIdx;
                                }
                            }
                            ImGui::EndDragDropTarget();
                            if (m_entries[rowIdx].isSelected)
                                ImGui::PopStyleColor();
                        }
                        ImGui::SameLine(0.0f, 0.0f);//no spacing
                        if (isIndented)
                        {
                            if (rowIdx == clipper.DisplayStart)
                                ImGui::Indent();
                            else if (m_entries[rowIdx].parentId == SongID::Invalid)
                            {
                                ImGui::Unindent();
                                isIndented = false;
                            }
                        }
                        else if (m_entries[rowIdx].parentId != SongID::Invalid)
                        {
                            ImGui::Indent();
                            isIndented = true;
                        }

                        if (m_entries[rowIdx].parentId != SongID::Invalid)
                        {
                            ImGui::Bullet();
                        }
                        else if (m_entries[rowIdx].isRoot)
                        {
                            if (ImGui::Checkbox("##CanMerge", &m_entries[rowIdx].canMerge))
                                m_entries[rowIdx].canMerge ? m_numMergedEntries++ : m_numMergedEntries--;
                            ImGui::SameLine();
                        }
                        if (m_isRenaming && m_renamedEntryIndex == rowIdx)
                        {
                            ImGui::SetScrollHereY();
                            ImGui::SetKeyboardFocusHere();
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            if (ImGui::InputText("##edit", &m_renamedString, ImGuiInputTextFlags_EnterReturnsTrue))
                            {
                                m_isRenaming = false;

                                auto oldFileName = songs.GetFullpath(m_entries[rowIdx].song);
                                if (io::File::IsExisting(oldFileName.c_str()))
                                {
                                    m_entries[rowIdx].song->Edit()->name = m_renamedString;
                                    auto newFileName = songs.GetFullpath(song);
                                    if (!io::File::Move(oldFileName.c_str(), newFileName.c_str()))
                                    {
                                        io::File::Copy(oldFileName.c_str(), newFileName.c_str());
                                        Log::Warning("Merge: Can't rename file \"%s\"\n", oldFileName.c_str());
                                        songs.m_hasFailedDeletes = true;
                                    }
                                    else
                                        Log::Message("Merge: \"%s\" renamed to \"%s\"\n", oldFileName.c_str(), newFileName.c_str());
                                }
                            }
                            else if (ImGui::IsItemDeactivated())
                                m_isRenaming = false;
                        }
                        else
                            ImGui::TextUnformatted(song->GetName());

                        // artists
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(songs.m_db.GetArtists(song->GetId()).c_str());

                        // extension
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(song->GetType().GetExtension());

                        // duration
                        ImGui::TableNextColumn();
                        for (uint16_t i = 0; i <= song->GetLastSubsongIndex(); i++)
                        {
                            if (!song->IsSubsongDiscarded(i))
                            {
                                auto subsongDurationCs = song->GetSubsongDurationCs(i);
                                ImGui::Text("%u:%02u  ", subsongDurationCs / 6000, (subsongDurationCs / 100) % 60);
                                break;
                            }
                        }

                        // file size
                        ImGui::TableNextColumn();
                        auto sizeFormat = GetSizeFormat(song->GetFileSize());
                        ImGui::Text("%.2f%s  ", song->GetFileSize() / sizeFormat.second, sizeFormat.first);

                        // file crc
                        ImGui::TableNextColumn();
                        ImGui::Text("%08X  ", song->GetFileCrc());

                        // internal id
                        ImGui::TableNextColumn();
                        ImGui::Text("%06X  ", uint32_t(song->GetId()));

                        // source name
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", song->GetSourceName());

                        ImGui::PopID();
                    }
                    if (isIndented)
                        ImGui::Unindent();
                }

                if (m_isRenaming == false && m_lastSelectedEntryIndex >= 0 && m_entries[m_lastSelectedEntryIndex].parentId == SongID::Invalid && ImGui::IsKeyPressed(ImGuiKey_F2))
                {
                    m_isRenaming = true;
                    m_renamedString = m_entries[m_lastSelectedEntryIndex].song->GetName();
                    m_renamedEntryIndex = m_lastSelectedEntryIndex;
                }

                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_F7))
                    {
                        auto lastCheckedEntry = m_lastCheckedEntry + m_entries.NumItems();
                        for (uint32_t i = 0, e = m_entries.NumItems(); i < e; i++)
                        {
                            uint32_t index = (lastCheckedEntry - i - 1) % e;
                            if (m_entries[index].isRoot)
                            {
                                float item_pos_y = clipper.StartPosY + clipper.ItemsHeight * index;
                                ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetWindowPos().y);
                                m_lastCheckedEntry = index;
                                break;
                            }
                        }
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_F8))
                    {
                        auto lastCheckedEntry = m_lastCheckedEntry;
                        for (uint32_t i = 0, e = m_entries.NumItems(); i < e; i++)
                        {
                            uint32_t index = (lastCheckedEntry + i + 1) % e;
                            if (m_entries[index].isRoot)
                            {
                                float item_pos_y = clipper.StartPosY + clipper.ItemsHeight * index;
                                ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetWindowPos().y);
                                m_lastCheckedEntry = index;
                                break;
                            }
                        }
                    }
                }

                ImGui::EndTable();

                Unmerge(unmergedEntryIndex);
                Remerge(droppedEntryIndex);

                ImGui::BeginDisabled(m_numMergedEntries == 0);
                char mergeLabel[64];
                if (m_numMergedEntries > 0)
                    sprintf(mergeLabel, "Merge %u song%s###Merge", m_numMergedEntries, m_numMergedEntries > 1 ? "s" : "");
                if (ImGui::Button(m_numMergedEntries > 0 ? mergeLabel : "Nothing to merge###Merge", ImVec2(-FLT_MIN, 0)))
                {
                    Process(songs);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
            }

            ImGui::EndPopup();
        }
    }

    void Library::SongsUI::SongMerger::Unmerge(int32_t unmergedEntryIndex)
    {
        if (unmergedEntryIndex >= 0)
        {
            auto& entry = m_entries[unmergedEntryIndex];
            if (entry.isRoot)
            {
                if (entry.canMerge)
                    m_numMergedEntries--;
                else
                    entry.canMerge = false;
                entry.isRoot = false;
                for (;;)
                {
                    unmergedEntryIndex++;
                    if (unmergedEntryIndex == m_entries.NumItems<int32_t>() || m_entries[unmergedEntryIndex].parentId == SongID::Invalid)
                        break;
                    m_entries[unmergedEntryIndex].parentId = SongID::Invalid;
                }
            }
            else if (entry.parentId != SongID::Invalid)
            {
                entry.parentId = SongID::Invalid;
                for (;;)
                {
                    auto nextEntryIndex = unmergedEntryIndex + 1;
                    if (nextEntryIndex == m_entries.NumItems<int32_t>() || m_entries[nextEntryIndex].parentId == SongID::Invalid)
                    {
                        if (m_entries[unmergedEntryIndex - 1].isRoot)
                        {
                            if (m_entries[unmergedEntryIndex - 1].canMerge)
                                m_numMergedEntries--;
                            else
                                m_entries[unmergedEntryIndex - 1].canMerge = false;
                            m_entries[unmergedEntryIndex - 1].isRoot = false;
                        }
                        break;
                    }
                    std::swap(m_entries[unmergedEntryIndex], m_entries[nextEntryIndex]);
                    unmergedEntryIndex = nextEntryIndex;
                }
            }
        }
    }

    void Library::SongsUI::SongMerger::Remerge(int32_t rootEntryIndex)
    {
        // lazy code here...
        if (rootEntryIndex >= 0 && !m_entries[rootEntryIndex].isSelected)
        {
            // Unmerge (in reverse order to avoid shuffle)
            auto rootSong = m_entries[rootEntryIndex].song;
            for (int64_t i = m_entries.NumItems() - 1; i >= 0; i--)
            {
                if (m_entries[i].isSelected)
                    Unmerge(int32_t(i));
            }

            // count selected song before and after the drop point
            int32_t numBeforeRoot = 0;
            int32_t numAfterRoot = 0;
            rootEntryIndex = 0x7fFFffFF;
            for (int32_t i = 0; i < m_entries.NumItems<int32_t>(); i++)
            {
                if (m_entries[i].isSelected)
                {
                    if (i < rootEntryIndex)
                        numBeforeRoot++;
                    else
                        numAfterRoot++;
                }
                else if (rootEntryIndex == 0x7fFFffFF && m_entries[i].song == rootSong)
                    rootEntryIndex = i;
            }

            // reorder the entries in a new array (move selected after the drop point)
            Array<Entry> entries;
            entries.Resize(m_entries.NumItems());
            int32_t ofs = rootEntryIndex - numBeforeRoot + 1;
            for (int32_t i = 0, j = 0; i < m_entries.NumItems<int32_t>(); i++)
            {
                if (!m_entries[i].isSelected)
                {
                    entries[j++] = m_entries[i];
                    if (i == rootEntryIndex)
                        j += numBeforeRoot + numAfterRoot;
                }
                else
                    entries[ofs++] = m_entries[i];
            }

            // tag the selected entries as children and update the root if necessary
            rootEntryIndex -= numBeforeRoot;
            SongID parentId;
            if (entries[rootEntryIndex].parentId != SongID::Invalid)
                parentId = entries[rootEntryIndex].parentId;
            else
            {
                parentId = entries[rootEntryIndex].song->GetId();
                if (!entries[rootEntryIndex].isRoot)
                {
                    m_numMergedEntries++;
                    entries[rootEntryIndex].isRoot = true;
                    entries[rootEntryIndex].canMerge = true;
                }
            }
            for (int32_t i = 0; i < numBeforeRoot + numAfterRoot; i++)
                entries[rootEntryIndex + i + 1].parentId = parentId;

            // replace the old entries with the reordered ones
            m_entries = std::move(entries);
        }
    }

    void Library::SongsUI::SongMerger::Process(SongsUI& songs)
    {
        auto& library = songs.GetLibrary();
        for (uint32_t i = 0, numEntries = m_entries.NumItems(); i < numEntries;)
        {
            auto& entry = m_entries[i];
            if (entry.isRoot && entry.canMerge)
            {
                auto* primarySong = entry.song->Edit();

                for (++i; i < numEntries && m_entries[i].parentId != SongID::Invalid; ++i)
                {
                    auto* song = m_entries[i].song->Edit();
                    auto filename = songs.GetFullpath(m_entries[i].song);
                    if (!io::File::Delete(filename.c_str()))
                    {
                        Log::Warning("Can't delete file \"%s\"\n", filename.c_str());
                        songs.InvalidateCache();
                    }

                    Log::Message("Merge: ID_%06X \"[%s]%s\" with ID_%06X \"[%s]%s\"\n", uint32_t(song->id), song->type.GetExtension(), songs.m_db.GetTitleAndArtists(song->id).c_str()
                        , uint32_t(primarySong->id), primarySong->type.GetExtension(), songs.m_db.GetTitleAndArtists(primarySong->id).c_str());

                    for (auto sourceId : song->sourceIds)
                    {
                        if (sourceId.sourceId != SourceID::FileImportID)
                            primarySong->sourceIds.Add(sourceId);
                        library.m_sources[sourceId.sourceId]->DiscardSong(sourceId, primarySong->id);
                    }

                    songs.m_db.RemoveSong(song->id);
                    for (uint16_t j = 0; j <= song->lastSubsongIndex; j++)
                    {
                        song->subsongs[j].isDiscarded = true;
                        SubsongID subsongId(song->id, j);
                        Core::Discard(MusicID(subsongId, DatabaseID::kLibrary));
                    }

                    for (auto artistId : song->artistIds)
                    {
                        auto* artist = library.m_db[artistId]->Edit();
                        if (--artist->numSongs == 0)
                            library.m_db.RemoveArtist(artistId);
                    }

                    library.m_db.Raise(Database::Flag::kSaveArtists | Database::Flag::kSaveSongs);
                }
            }
            else
                i++;
        }
    }
}
// namespace rePlayer