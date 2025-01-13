#pragma once

#include <Database/DatabaseSongsUI.h>
#include <Library/Library.h>

namespace rePlayer
{
    class Library::SongsUI : public DatabaseSongsUI
    {
    public:
        SongsUI(Window& owner);
        ~SongsUI() override;

        void OnDisplay() override;
        void OnEndUpdate() override;

        Library& GetLibrary();

    protected:
        void OnAddingArtistToSong(Song* song, ArtistID artistId) override;
        void OnSelectionContext() override;

    private:
        class SongMerger;

    private:
        SongMerger* m_songMerger = nullptr;
    };
}
// namespace rePlayer