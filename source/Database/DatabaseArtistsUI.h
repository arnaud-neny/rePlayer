#pragma once

#include <Core.h>
#include <Database/Types/Artist.h>
#include <Database/Types/MusicID.h>
struct ImGuiTextFilter;

namespace core
{
    class Window;
}
// namespace core

namespace rePlayer
{
    using namespace core;

    class Database;

    class DatabaseArtistsUI
    {
    public:
        DatabaseArtistsUI(DatabaseID databaseId, Window& owner);
        virtual ~DatabaseArtistsUI();

        void MakeDirty(bool isDirty = true);

        void OnDisplay();

    protected:
        virtual void SourcesUI(Artist* selectedArtist);
        virtual void OnSavedChanges(Artist* selectedArtist);

    private:
        static void OnArtistFilterLoaded(uintptr_t userData, void*, const char*);

        void IdUI(Artist* selectedArtist);
        void HandleUI(Artist* selectedArtist);
        void AliasUI(Artist* selectedArtist);
        void NameUI(Artist* selectedArtist);
        void GroupUI(Artist* selectedArtist);
        void CountryUI(Artist* selectedArtist);
        void SaveChangesUI(Artist* selectedArtist);
        void MergeUI(Artist* selectedArtist);
        void SongUI(Artist* selectedArtist);

        bool SelectMasterArtist(ArtistID artistId);

    protected:
        Database& m_db;
        Window& m_owner;
        const DatabaseID m_databaseId;

        Array<ArtistID> m_artists;
        ImGuiTextFilter* m_artistFilter;

        ArtistSheet m_selectedArtistCopy;
        int32_t m_selectedCountry = -1;

        uint32_t m_dbRevision = 0;

        struct
        {
            ImGuiTextFilter* filter;
            Array<ArtistID> artists;
            Artist* masterArtist = nullptr;
            Artist* mergedArtist = nullptr;
            bool isActive = false;
        } m_artistMerger;
    };
}
// namespace rePlayer