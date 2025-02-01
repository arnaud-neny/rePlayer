#pragma once

#include <Database/DatabaseSongsUI.h>

namespace rePlayer
{
    using namespace core;

    class Database;

    class DatabaseArtistsUI : public DatabaseSongsUI
    {
    public:
        DatabaseArtistsUI(DatabaseID databaseId, Window& owner);
        virtual ~DatabaseArtistsUI();

        void OnDisplay();

    protected:
        Array<SubsongEntry> GatherEntries() const override;

        virtual void SourcesUI(Artist* selectedArtist);
        virtual void OnSavedChanges(Artist* selectedArtist);

    private:
        static void OnArtistFilterLoaded(uintptr_t userData, void*, const char*);
        static void OnArtistSelectionLoaded(uintptr_t userData, void*, const char*);

        void HandleUI();
        void AliasUI();
        void NameUI();
        void GroupUI();
        void CountryUI();
        void SaveChangesUI(Artist* selectedArtist);
        void MergeUI();
        void SongUI();

        bool SelectMasterArtist(ArtistID artistId);

    protected:
        Array<ArtistID> m_artists;
        ImGuiTextFilter* m_artistFilter;

        ArtistSheet m_selectedArtistCopy;

        uint32_t m_dbArtistsRevision = 0;

        struct
        {
            ImGuiTextFilter* filter;
            Array<ArtistID> artists;
            ArtistID masterArtistId;
            ArtistID mergedArtistId;
            bool isActive = false;
        } m_artistMerger;
    };
}
// namespace rePlayer