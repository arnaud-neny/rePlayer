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

        void InvalidateCache();
        void CleanupCache();

    private:
        std::string GetDirectory(Song* song, Array<Artist*>* artists = nullptr) const;

    private:
        bool m_hasFailedDeletes = false;
    };
}
// namespace rePlayer