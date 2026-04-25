// Core
#include <Core/String.h>
#include <ImGui.h>

// rePlayer
#include <Library/LibraryDatabase.h>
#include <Playlist/Playlist.h>
#include <RePlayer/Core.h>
#include <UI/BusySpinner.h>

#include "LibraryBrowserUI.h"

#include <algorithm>

namespace rePlayer
{
    Library::BrowserUI::BrowserUI(Library& library)
        : m_context(new BrowserContext{
            .busySpinner = library.m_busySpinner
        })
    {
        library.RegisterSerializedCustomData(this, [](void* data, const char* line)
        {
            auto* This = reinterpret_cast<Library::BrowserUI*>(data);
            if (strstr(line, kHeader))
            {
                line += sizeof(kHeader)  - 1;
                uint32_t value;
                if (sscanf_s(line, "Count=%u", &value) == 1)
                {
                    This->m_filters.Resize(value);
                    for (auto& filter : This->m_filters)
                        filter = new ImGuiTextFilter();
                    return true;
                }
                else if (sscanf_s(line, "%u=", &value) == 1)
                {
                    if (auto* c = strstr(line, "="))
                    {
                        strcpy_s(This->m_filters[value]->InputBuf, c + 1);
                        This->m_filters[value]->Build();
                        return true;
                    }
                }
            }
            return false;

        }, [](void* data, ImGuiTextBuffer* buf)
        {
            auto* This = reinterpret_cast<Library::BrowserUI*>(data);
            auto numItems = This->m_filters.NumItems();
            buf->appendf("%sCount=%u\n", kHeader, numItems);
            for (uint32_t i = 0; i < numItems; i++)
                buf->appendf("%s%u=%s\n", kHeader, i, This->m_filters[i]->InputBuf);
        });
        m_stages.Add({ "Root/", 0 });
    }

    Library::BrowserUI::~BrowserUI()
    {
        for (auto* filter : m_filters)
            delete filter;
        delete m_context;
    }

    void Library::BrowserUI::OnDisplay()
    {
        auto& library = Core::GetLibrary();
        auto& db = Core::GetLibraryDatabase();

        library.m_busySpinner->Begin();

        bool isDirty = DisplayStages();

        // title prefix
        m_title = "browsing ";
        if (m_context->sourceId != SourceID::NumSourceIDs)
            m_title += SourceID::sourceNames[m_context->sourceId];

        // if busy spinner is valid, then the browser is disabled, so keep displaying the source folders
        if (m_context->sourceId == SourceID::NumSourceIDs || library.m_busySpinner.IsValid())
        {
            int numSources = 0;
            for (int i = 0; i < SourceID::NumSourceIDs; ++i)
            {
                auto sourceId = SourceID::sortedSources[i];
                if (library.m_sources[sourceId]->m_canBrowse)
                {
                    std::string name = ImGuiIconDrive;
                    name += SourceID::sourceNames[sourceId];
                    if (ImGui::Selectable(name.c_str(), m_context->sourceId == sourceId, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0))
                    {
                        m_context->sourceId = sourceId;
                        m_context->stage.Next();
                        library.m_sources[sourceId]->BrowserInit(*m_context);
                        m_stages.Add({ SourceID::sourceNames[sourceId], 0 });
                        m_stages.Last().name += '/';
                    }
                    numSources++;
                }
            }
            if (m_context->sourceId == SourceID::NumSourceIDs)
            {
                char buf[32];
                sprintf(buf, "%d repositories", numSources);
                m_title += buf;
            }
        }
        else
        {
            // dirty if database has changed
            isDirty |= (m_context->stage.HasArtists() && m_dbArtistsRevision != db.ArtistsRevision())
                || (m_context->stage.HasSongs() && m_dbSongsRevision != db.SongsRevision());
            m_dbArtistsRevision = db.ArtistsRevision();
            m_dbSongsRevision = db.SongsRevision();

            auto& source = *library.m_sources[m_context->sourceId];
            isDirty = DisplayFilter(source, isDirty);

            DisplayEntries(source, isDirty);
        }

        library.m_busySpinner->End();
    }

    const char* Library::BrowserUI::GetTitle() const
    {
        return m_title.c_str();
    }

    bool Library::BrowserUI::DisplayStages()
    {
        bool isDirty = false;
        for (int32_t i = 0; i < m_stages.NumItems<int32_t>(); ++i)
        {
            ImGui::PushID(i);
            if (i != 0)
                ImGui::SameLine(0.0f, 0.0f);
            if (ImGui::Button(m_stages[i].name.c_str()) && (i + 1) != m_stages.NumItems<int32_t>())
            {
                m_stages.Resize(i + 1);
                if (i == 0)
                    m_context->sourceId = SourceID::NumSourceIDs;
                m_context->stage = { BrowserContext::Stage::Id(i), BrowserContext::Stage::Type::None };
                m_context->stageDbIndex = m_stages[i].dbIndex;
                m_context->entries.Clear();
                m_lastSelectedEntry = -1;
                isDirty = true;
            }
            ImGui::PopID();
        }
        return isDirty;
    }

    bool Library::BrowserUI::DisplayFilter(Source& source, bool isDirty)
    {
        isDirty |= m_context->stage.type == BrowserContext::Stage::Type::None;
        while (m_filters.NumItems() < uint32_t(m_context->stage.id))
            m_filters.Add(new ImGuiTextFilter());
        auto& filter = *m_filters[int(m_context->stage.id) - 1];
        if (filter.Draw("##filter", -1) || isDirty)
            source.BrowserPopulate(*m_context, filter);
        return isDirty;
    }

    void Library::BrowserUI::DisplayEntries(Source& source, bool isDirty)
    {
        auto& context = *m_context;
        assert(context.stage != Source::kStageRoot);

        if (ImGui::BeginTable(SourceID::sourceNames[0], context.numColumns, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg))
        {
            for (int32_t i = 0; i < context.numColumns; ++i)
                ImGui::TableSetupColumn(context.columnNames[i], ImGuiTableColumnFlags_WidthStretch | (context.disabledColumns & (1 << i) ? ImGuiTableColumnFlags_Disabled : ImGuiTableColumnFlags_None), 0.0f, i);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            auto* sortsSpecs = ImGui::TableGetSortSpecs();
            if (sortsSpecs->SpecsDirty || isDirty)
            {
                std::sort(context.entries.begin(), context.entries.end(), [&](auto& l, auto& r)
                {
                    for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                    {
                        auto& sortSpec = sortsSpecs->Specs[i];
                        int64_t delta = source.BrowserCompare(context, l, r, BrowserContext::Column(sortSpec.ColumnUserID));
                        if (delta)
                            return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                    }
                    return l.dbIndex < r.dbIndex;
                });
                sortsSpecs->SpecsDirty = false;
            }

            bool updateStage = false;

            ImGuiListClipper clipper;
            clipper.Begin(context.entries.NumItems<int>());
            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    auto& rowEntry = context.entries[rowIdx];

                    ImGui::TableNextColumn();
                    ImGui::PushID(rowIdx);

                    if (ImGui::Selectable("##select", context.entries[rowIdx].isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            auto isSong = rowEntry.isSong;
                            if (isSong)
                            {
                                auto songs = source.BrowserFetchSongs(context, rowEntry);
                                if (isSong = songs.IsNotEmpty())
                                    Core::GetPlaylist().ProcessBrowserSong(songs.Last(), Playlist::kPlay);
                            }
                            if (!isSong)
                            {
                                context.stageDbIndex = rowEntry.dbIndex;
                                updateStage = true;
                                m_stages.Add({ source.BrowserGetStageName(rowEntry, context.stage), rowEntry.dbIndex });
                                m_stages.Last().name += '/';
                            }
                        }
                        if (ImGui::GetIO().KeyShift)
                        {
                            if (m_lastSelectedEntry < 0)
                                m_lastSelectedEntry = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                for (auto& entry : context.entries)
                                    entry.isSelected = false;
                            }
                            auto start = rowIdx;
                            auto end = m_lastSelectedEntry;
                            if (start > end)
                                std::swap(start, end);
                            for (; start <= end; start++)
                                context.entries[start].isSelected = true;
                        }
                        else
                        {
                            m_lastSelectedEntry = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                for (auto& entry : context.entries)
                                    entry.isSelected = false;
                            }
                            rowEntry.isSelected = ~rowEntry.isSelected;
                        }
                    }
                    // Context menu
                    if (ImGui::BeginPopupContextItem("SelectPopup"))
                    {
                        if (ImGui::Selectable("Invert selection"))
                        {
                            for (auto& si : context.entries)
                                si.isSelected = ~si.isSelected;
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Add to playlist"))
                        {
                            rowEntry.isSelected = true;
                            for (auto& entry : context.entries)
                            {
                                if (entry.isSelected)
                                {
                                    auto songs = source.BrowserFetchSongs(context, entry);
                                    for (auto& song : songs)
                                        Core::GetPlaylist().ProcessBrowserSong(song, Playlist::kEnqueue);
                                }
                            }
                        }
                        if (ImGui::Selectable("Import"))
                        {
                            rowEntry.isSelected = true;
                            auto& library = Core::GetLibrary();
                            SourceResults collectedSongs;
                            for (auto& entry : context.entries)
                            {
                                if (entry.isSelected)
                                    source.BrowserImport(context, entry, collectedSongs);
                            }
                            if (collectedSongs.songs.IsNotEmpty())
                            {
                                library.m_imports = {};
                                library.m_importArtists.isExternal = true;
                                library.m_importArtists.isOpened = true;
                                library.m_imports.isOpened = &library.m_importArtists.isOpened;
                                library.m_imports.sourceResults = std::move(collectedSongs);
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine(0.0f, 0.0f);
                    // Display all columns
                    source.BrowserDisplay(context,rowEntry, BrowserContext::Column::Name);
                    for (int32_t column = 1; column < context.numColumns; ++column)
                    {
                        ImGui::TableNextColumn();
                        source.BrowserDisplay(context, rowEntry, BrowserContext::Column(column));
                    }
                    ImGui::PopID();
                }
            }

            if (updateStage)
            {
                context.entries.Clear();
                context.stage.Next();
                m_lastSelectedEntry = -1;
            }

            ImGui::EndTable();
        }
    }
}
// namespace rePlayer