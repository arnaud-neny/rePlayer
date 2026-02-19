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

        void OnDisplay() override;

    protected:
        Array<SubsongEntry> GatherEntries() const override;
        void OnArtistContext(int32_t entryIndex) override;

        virtual void SourcesUI(Artist* selectedArtist);
        virtual void OnSavedChanges(Artist* selectedArtist);

        ArtistID PickArtist(ArtistID skippedArtistId);

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

    protected:
        Array<ArtistID> m_artists;
        ImGuiTextFilter* m_artistFilter;

        struct
        {
            ImGuiTextFilter* filter;
            Array<ArtistID> artists;
            uint32_t revision = 0;
            bool isOpened = false;
        } m_artistPicker;

        ArtistSheet m_selectedArtistCopy;

        uint32_t m_dbArtistsRevision = 0;
        SubsongID m_lastTrackedSubsongId;

        struct
        {
            ArtistID masterArtistId;
            ArtistID mergedArtistId;
            bool isActive = false;
        } m_artistMerger;
    };
}
// namespace rePlayer