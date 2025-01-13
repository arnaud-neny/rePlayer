#pragma once

#include <Core.h>
#include <Containers/Array.h>
#include <Containers/HashMap.h>
#include <Database/Types/MusicID.h>
#include <Database/Types/Tags.h>

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
    class Export;
    class Song;

    class DatabaseSongsUI
    {
    public:
        DatabaseSongsUI(DatabaseID databaseId, Window& owner);
        virtual ~DatabaseSongsUI();

        void TrackSubsong(SubsongID subsongId);
        void DeleteSubsong(SubsongID subsongId);
        bool HasDeletedSubsongs(SongID songId) const;

        uint32_t NumSubsongs() const;
        uint32_t NumSelectedSubsongs() const;

        virtual void OnDisplay();
        virtual void OnEndUpdate();

    protected:
        virtual void OnAddingArtistToSong(Song* song, ArtistID artistId);
        virtual void OnSelectionContext();

    private:
        static void OnSongFilterLoaded(uintptr_t userData, void*, const char*);

        void FilteringUI(bool& isDirty);
        void SongsUI(bool isDirty);

        void ExportAsWavUI();

        // SongUI
        void FilterBarsUI(bool& isDirty);
        void SortSubsongs(bool isDirty);
        void SetRowBackground(int32_t rowIdx, Song* song, SubsongID subsongId, MusicID currentPlayingSong);
        void Selection(int32_t rowIdx, MusicID musicId);

        // Selection
        bool Select(int32_t rowIdx, MusicID musicId, bool isSelected);
        void DragSelection();
        void SelectionContext(bool isSelected);

        // Selection Context
        void AddToArtist();
        void EditTags();
        bool Discard();
        void ResetReplay();
        void ResetSubsongState();

    private:
        enum TabIDs
        {
            kTitle,
            kArtists,
            kType,
            kSize,
            kDuration,
            kYear,
            kCRC,
            kState,
            kRating,
            kDatabaseDate,
            kSource,
            kReplay,
            kNumIDs
        };

    protected:
        Database& m_db;
        Window& m_owner;
        const DatabaseID m_databaseId;

        using FilterFlagType = uint32_t;
        enum class FilterFlag : FilterFlagType
        {
            kNone           = 0,
            kSongTitle      = FilterFlagType(1) << 0,
            kArtistHandle   = FilterFlagType(1) << 1,
            kSubongTitle    = FilterFlagType(1) << 2,
            kArtistName     = FilterFlagType(1) << 3,
            kArtistAlias    = FilterFlagType(1) << 4,
            kArtistCountry  = FilterFlagType(1) << 5,
            kArtistGroup    = FilterFlagType(1) << 6,
            kSongTag        = FilterFlagType(1) << 7,
            kSongType       = FilterFlagType(1) << 8,
            kReplay         = FilterFlagType(1) << 9,
            kSource         = FilterFlagType(1) << 10
        };
        friend constexpr FilterFlag operator|(FilterFlag a, FilterFlag b);
        friend constexpr FilterFlag& operator^=(FilterFlag& a, FilterFlagType b);
        friend constexpr bool operator&&(FilterFlag a, FilterFlag b);

        static constexpr uint32_t kNumFilterFlags = 11;
        static constexpr uint32_t kMaxFilters = 8;
        struct Filter
        {
            FilterFlag flags;
            uint32_t id;
            ImGuiTextFilter* ui;
        };
        Array<Filter> m_filters;
        Array<uint32_t> m_filterLostIds;

        struct SubsongEntry
        {
            SubsongID id;
            bool isSelected = false;

            bool operator==(SubsongID otherId) const;
        };
        Array<SubsongEntry> m_entries;
        uint32_t m_numSelectedEntries = 0;

        uint32_t m_dbRevision = 0;

        SubsongID m_lastSelectedSubsong;
        SubsongID m_trackedSubsongId;

        HashMap<SubsongID, float> m_subsongHighlights;

        bool m_isExportAsWavTriggered = false;
        Export* m_export = nullptr;

        Array<SubsongID> m_deletedSubsongs;
    };
}
// namespace rePlayer

#include "DatabaseSongsUI.inl.h"