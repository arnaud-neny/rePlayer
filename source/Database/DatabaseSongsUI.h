#pragma once

#include <Core.h>
#include <Containers/Array.h>
#include <Containers/HashMap.h>
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
    class Export;
    class Song;

    class DatabaseSongsUI
    {
    public:
        DatabaseSongsUI(DatabaseID databaseId, Window& owner, bool isScrollingEnabled = true, uint16_t defaultHiddenColumns = (1 << kSize) + (1 << kYear) + (1 << kCRC) + (1 << kDatabaseDate) + (1 << kSource) + (1 << kReplay), const char* header = "Songs");
        virtual ~DatabaseSongsUI();

        void TrackSubsong(SubsongID subsongId);

        uint32_t NumSubsongs() const;
        uint32_t NumSelectedSubsongs() const;

        virtual void OnDisplay();
        virtual void OnEndUpdate();

    protected:
        struct SubsongEntry : public SubsongID
        {
            bool IsSelected() const;
            void Select(bool isEnabled);
        };

    protected:
        virtual Array<SubsongEntry> GatherEntries() const;
        virtual void OnSelectionContext();

    private:
        void DisplaySongsFilter(bool& isDirty);
        void DisplaySongsTable(bool& isDirty);
        void DisplayExportAsWav();

        // Used in DisplaySongsFilter
        void DisplaySongsFilterUI(bool& isDirty);
        void FilterSongs();

        // Used in DisplaySongsTable
        void SortSubsongs(bool isDirty);
        void UpdateRowBackground(int32_t rowIdx, Song* song, SubsongID subsongId, MusicID currentPlayingSong);
        void UpdateSelection(int32_t rowIdx, MusicID musicId);

        // Used in UpdateSelection
        bool Select(int32_t rowIdx, MusicID musicId, bool isSelected);
        void DragSelection();
        void UpdateSelectionContext(bool isSelected);

        // Used in UpdateSelectionContext
        void AddToArtist();
        void RemoveFromArtistUI();
        void EditTags();
        bool Discard();
        void ResetReplay();
        void ResetSubsongState();

        void RemoveFromArtist();

    protected:
        Database& m_db;
        Window& m_owner;
        const std::string m_header;
        const DatabaseID m_databaseId;

        uint32_t m_dbSongsRevision = 0;

        // filter

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

        // table

        enum TabIDs
        {
            kTitle, kArtist, kType, kSize, kDuration, kYear, kCRC, kState, kRating, kDatabaseDate, kSource, kReplay,
            kNumIDs
        };

        HashMap<SubsongID, float> m_subsongHighlights;
        const uint16_t m_defaultHiddenColumns;

        // entries

        Array<SubsongEntry> m_entries;
        uint32_t m_numSelectedEntries = 0;

        const bool m_isScrollingEnabled = true;

        SubsongID m_trackedSubsongId;

        // selection
        SubsongID m_lastSelectedSubsong;
        ArtistID m_artistToRemove = ArtistID::Invalid;

        // WAV export
        bool m_isExportAsWavTriggered = false;
        Export* m_export = nullptr;
    };
}
// namespace rePlayer

#include "DatabaseSongsUI.inl.h"