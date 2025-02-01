#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Database/Types/Song.h>
#include <Library/LibrarySongsUI.h>

namespace rePlayer
{
    class Database;

    template <typename ParentDatabaseUI>
    class Library::DatabaseUI<ParentDatabaseUI>::SongMerger
    {
    public:
        void MenuItem(DatabaseUI<ParentDatabaseUI>& songs);
        void Update(DatabaseUI<ParentDatabaseUI>& songs);

    private:
        struct Entry
        {
            SmartPtr<Song> song;
            bool isRoot = false;
            bool isSelected = false;
            bool canMerge = false;
            SongID parentId = SongID::Invalid;

            bool operator==(SongID id) const { return song->GetId() == id; }
        };

    private:
        void Unmerge(int32_t unmergedEntryIndex);
        void Remerge(int32_t rootEntryIndex);
        void Process(DatabaseUI<ParentDatabaseUI>& songs);

    public:
        Array<Entry> m_entries;
        bool m_isStarted = false;
        bool m_isOpened = false;
        bool m_isRenaming = false;

        std::string m_renamedString;
        int32_t m_renamedEntryIndex;

        int32_t m_lastSelectedEntryIndex = -1;
        uint32_t m_numMergedEntries = 0;

        uint32_t m_lastCheckedEntry = 0;

        int32_t m_frame = 0;
    };
}
// namespace rePlayer