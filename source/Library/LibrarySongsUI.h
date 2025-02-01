#pragma once

#include <Database/DatabaseSongsUI.h>
#include <Library/LibraryDatabaseUI.h>

namespace rePlayer
{
    class Library::SongsUI : public Library::DatabaseUI<DatabaseSongsUI>
    {
    public:
        SongsUI(Window& owner);
    };
}
// namespace rePlayer