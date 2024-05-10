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

        uint32_t NumSubsongs() const;
        uint32_t NumSelectedSubsongs() const;

        virtual void OnDisplay();
        virtual void OnEndUpdate();

        virtual std::string GetFullpath(Song* song) const;

        virtual void InvalidateCache();

    protected:
        virtual void OnAddingArtistToSong(Song* song, ArtistID artistId);
        virtual void OnSelectionContext();

    private:
        static void OnSongFilterLoaded(uintptr_t userData, void*, const char*);

        void FilteringUI(bool& isDirty);
        void SongsUI(bool isDirty);

        void ExportAsWavUI();

        // SongUI
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
            kNumIDs
        };

    protected:
        Database& m_db;
        Window& m_owner;
        const DatabaseID m_databaseId;

        Tag m_filterTags = Tag::kNone;
        enum class TagMode : uint32_t
        {
            Disable,
            Match,
            Inclusive,
            Exclusive
        } m_tagMode = TagMode::Disable;
        enum class FilterMode : uint32_t
        {
            All,
            Songs,
            Artists
        };
        FilterMode m_filterMode = FilterMode::All;
        ImGuiTextFilter* m_songFilter;

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