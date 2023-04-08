#pragma once

#include <Database/DatabaseArtistsUI.h>
#include <Library/Library.h>

namespace rePlayer
{
    class Library::ArtistsUI : public DatabaseArtistsUI
    {
    public:
        ArtistsUI(Window& owner);
        ~ArtistsUI() override;

        Library& GetLibrary();

    protected:
        void SourcesUI(Artist* selectedArtist) override;
        void OnSavedChanges(Artist* selectedArtist) override;
    };
}
// namespace rePlayer