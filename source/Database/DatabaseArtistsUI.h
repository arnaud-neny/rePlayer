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

        void OnDisplay();

    protected:
        virtual void SourcesUI(Artist* selectedArtist);
        virtual void OnSavedChanges(Artist* selectedArtist);

    private:
        enum TabIDs
        {
            kTitle,
            kArtists,
            kType,
            kNumIDs
        };

        enum class States : uint32_t
        {
            kNone = 0,
            kRemoveArtist = 1 << 0
        };
        friend constexpr States& operator|=(States& a, States b);
        friend constexpr bool operator&&(States a, States b);

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

        // SongUI
        void SortSongs(bool isDirty);
        States Selection(int32_t entryIdx, MusicID musicId, Artist* selectedArtist);
        void RemoveArtist(Artist* selectedArtist);

        // Selection
        bool Select(int32_t entryIdx, MusicID musicId, bool isSelected);
        void DragSelection();
        States SelectionContext(bool isSelected, Artist* selectedArtist);

    protected:
        Database& m_db;
        Window& m_owner;
        const DatabaseID m_databaseId;

        Array<ArtistID> m_artists;
        ImGuiTextFilter* m_artistFilter;

        ArtistSheet m_selectedArtistCopy;
        int32_t m_selectedCountry = -1;

        uint32_t m_dbRevision = 0;
        uint32_t m_dbSongsRevision = 0;

        struct SongEntry
        {
            SongID id;
            bool isSelected = false;

            bool operator==(SongID otherId) const;
        };
        Array<SongEntry> m_songEntries;
        uint32_t m_numSelectedSongEntries = 0;
        ArtistID m_selectedArtist;
        SongID m_lastSelectedSong;

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