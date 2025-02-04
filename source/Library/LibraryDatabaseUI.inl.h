// rePlayer
#include <Database/Types/SourceID.h>
#include <Library/LibrarySongMerger.inl.h>

#include "LibrarySongsUI.h"

namespace rePlayer
{
    template <typename ParentDatabaseUI>
    Library::DatabaseUI<ParentDatabaseUI>::DatabaseUI(Window& owner)
        : ParentDatabaseUI(DatabaseID::kLibrary, owner)
        , m_songMerger(new SongMerger())
    {}

    template <typename ParentDatabaseUI>
    Library::DatabaseUI<ParentDatabaseUI>::~DatabaseUI()
    {
        delete m_songMerger;
    }

    template <typename ParentDatabaseUI>
    void Library::DatabaseUI<ParentDatabaseUI>::OnDisplay()
    {
        ParentDatabaseUI::OnDisplay();
        m_songMerger->Update(*this);
    }

    template <typename ParentDatabaseUI>
    void Library::DatabaseUI<ParentDatabaseUI>::OnSelectionContext()
    {
        m_songMerger->MenuItem(*this);
    }

    template <typename ParentDatabaseUI>
    bool Library::DatabaseUI<ParentDatabaseUI>::AddToPlaylistUI()
    {
        if (!ImGui::GetIO().KeyShift)
            return ParentDatabaseUI::AddToPlaylistUI();

        bool isSelected = ImGui::Selectable("Add to playlist for Auto Merge");
        if (isSelected)
        {
            // group of sources that can be played together or not, and sorted by my merge preference
            static int32_t groups[] = {
                0,     // AmigaMusicPreservationSourceID
                1,     // TheModArchiveSourceID
                2,     // ModlandSourceID
                65536, // FileImportID
                0,     // HighVoltageSIDCollectionID
                0,     // SNDHID
                0,     // AtariSAPMusicArchiveID
                0,     // ZXArtID
                65537, // UrlImportID
                0      // VGMRips
            };
            static_assert(_countof(groups) == SourceID::NumSourceIDs);

            // gather selected entries
            Array<SubsongID> entries(0u, ParentDatabaseUI::m_numSelectedEntries);
            for (auto& entry : ParentDatabaseUI::m_entries)
            {
                if (entry.IsSelected())
                    entries.Add(entry);
            }
            // sort by group
            std::sort(entries.begin(), entries.end(), [&](auto l, auto r)
            {
                return groups[ParentDatabaseUI::m_db[l]->GetSourceId(0).sourceId] < groups[ParentDatabaseUI::m_db[r]->GetSourceId(0).sourceId];
            });
            // build spans of entries per group
            Array<uint32_t> spans(1, 6);
            spans[0] = 0;
            int32_t group = groups[ParentDatabaseUI::m_db[entries[0]]->GetSourceId(0).sourceId];
            for (uint32_t i = 1, e = entries.NumItems(); i < e; ++i)
            {
                if (group != groups[ParentDatabaseUI::m_db[entries[i]]->GetSourceId(0).sourceId])
                {
                    group = groups[ParentDatabaseUI::m_db[entries[i]]->GetSourceId(0).sourceId];
                    spans.Add(i);
                }
            }
            spans.Add(entries.NumItems());
            // randomize within each group and add to playlist
            for (int64_t i = 0, e = spans.NumItems() - 1; i < e; ++i)
            {
                for (int64_t begin = spans[i], end = spans[i + 1]; begin < end; ++begin)
                {
                    auto index = rand() % (end - begin);
                    std::swap(entries[begin], entries[begin + index]);
                    Core::Enqueue(MusicID(entries[begin], DatabaseID::kLibrary));
                }
            }
        }
        return isSelected;
    }
}
// namespace rePlayer