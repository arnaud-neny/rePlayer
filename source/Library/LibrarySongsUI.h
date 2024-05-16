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

        std::string GetFullpath(Song* song, Array<Artist*>* artists = nullptr) const override;
        std::string GetDirectory(Artist* artist) const;

        Library& GetLibrary();

        void InvalidateCache() override;
        void CleanupCache();

    protected:
        void OnAddingArtistToSong(Song* song, ArtistID artistId) override;
        void OnSelectionContext() override;

    private:
        class SongMerger;

    private:
        std::string GetDirectory(Song* song, Array<Artist*>* artists = nullptr) const;

    private:
        SongMerger* m_songMerger = nullptr;
        bool m_hasFailedDeletes = false;

        static const char* const ms_path;
    };
}
// namespace rePlayer