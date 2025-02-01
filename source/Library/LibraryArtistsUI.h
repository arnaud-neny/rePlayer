#pragma once

#include <Database/DatabaseArtistsUI.h>
#include <Library/LibraryDatabaseUI.h>

namespace rePlayer
{
    class Library::ArtistsUI : public Library::DatabaseUI<DatabaseArtistsUI>
    {
    public:
        ArtistsUI(Window& owner);
        ~ArtistsUI() override;

    private:
        Library& GetLibrary();

    protected:
        void SourcesUI(Artist* selectedArtist) override;
        void OnSavedChanges(Artist* selectedArtist) override;
    };
}
// namespace rePlayer