// rePlayer
//#include <Library/LibraryDatabase.h>
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
}
// namespace rePlayer