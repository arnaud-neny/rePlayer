#pragma once

#include <Database/Database.h>

namespace rePlayer
{
    class LibraryDatabase : public Database
    {
    public:
        ~LibraryDatabase() override;

        std::string GetFullpath(Song* song, Array<Artist*>* artists = nullptr) const override;
        std::string GetDirectory(Artist* artist) const;

        bool AddArtistToSong(Song* song, Artist* artist) override;

        void Update() override;

        void Patch();

    private:
        void CleanupCache();

        void DeleteInternal(Song* song, const char* logId) const override;
        void MoveInternal(const char* oldFilename, Song* song, const char* logId) const override;

        std::string GetDirectory(Song* song, Array<Artist*>* artists = nullptr) const;

    private:
        mutable bool m_hasFailedDeletes = false;
    };
}
// namespace rePlayer