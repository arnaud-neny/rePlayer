// rePlayer
#include <Library/LibraryDatabaseUI.inl.h>

#include "LibrarySongsUI.h"

namespace rePlayer
{
    Library::SongsUI::SongsUI(Window& owner)
        : Library::DatabaseUI<DatabaseSongsUI>(owner)
    {}
}
// namespace rePlayer