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
                return ParentDatabaseUI::m_db[l]->GetSourceId(0).AutoMerge() < ParentDatabaseUI::m_db[r]->GetSourceId(0).AutoMerge();
            });
            // build spans of entries per group
            Array<uint32_t> spans(1, SourceID::NumSourceIDs);
            spans[0] = 0;
            int32_t group = ParentDatabaseUI::m_db[entries[0]]->GetSourceId(0).AutoMerge();
            for (uint32_t i = 1, e = entries.NumItems(); i < e; ++i)
            {
                if (group != ParentDatabaseUI::m_db[entries[i]]->GetSourceId(0).AutoMerge())
                {
                    group = ParentDatabaseUI::m_db[entries[i]]->GetSourceId(0).AutoMerge();
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