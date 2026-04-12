#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

namespace rePlayer
{
    class SourceSNDH : public Source
    {
        struct Collector;
        struct Search;
        struct Composers;

    public:
        SourceSNDH();
        ~SourceSNDH() final;

        void FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner) final;
        void FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner) final;
        Import ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        std::string GetArtistStub(SourceID artistId) const final;

        bool IsValidationEnabled() const final;
        Status Validate(SourceID sourceId, SongID songId) final { return Source::Validate(sourceId, songId); }
        Status Validate(SourceID sourceId, ArtistID artistId) final;

        void Load() final;
        void Save() const final;

        void BrowserInit(BrowserContext& context) final;
        void BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter) final;
        int64_t BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const final;
        void BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const final;
        void BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs) final;
        std::string BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const final;
        Array<BrowserSong> BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry);

        static constexpr SourceID::eSourceID kID = SourceID::SNDHID;

    private:
        struct SongSource
        {
            uint32_t id;
            uint32_t crc = 0;
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t size : 31;
                    uint32_t isDiscarded : 1;//deleted or merged
                };
            };
            SongID songId = SongID::Invalid;
        };

        struct ArtistSource
        {
            union
            {
                uint32_t value = 0xffFFffFF;
                struct
                {
                    uint32_t isNotRegistered : 1;
                    uint32_t nameOffset : 31;
                };
                struct
                {
                    uint32_t isNext : 1;
                    uint32_t nextId : 31;
                };
            };
            uint32_t urlOffset;
        };

        struct SndhComposer
        {
            uint32_t url;
            uint32_t name;
            uint32_t songs;
        };

        struct SndhSong
        {
            uint32_t id;
            uint32_t name;
            uint32_t year;
            uint32_t next;
        };

        static constexpr BrowserColumn kColumnArtist = BrowserColumn(1);
        static constexpr BrowserColumn kColumnYear = BrowserColumn(2);
        static constexpr BrowserColumn kColumnId = BrowserColumn(3);
        static constexpr BrowserStage kStageArtists = { BrowserStageId(1), BrowserStageType::Folder };
        static constexpr BrowserStage kStageSongs = { BrowserStageId(2), BrowserStageType::Song };

    private:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        uint32_t FindArtist(const char* url, const char* name);

        template <typename T>
        void AddSong(const T& dbSong, const char* artistUrl, const char* artistName, SourceResults& collectedSongs, bool isNewChecked);

        bool DownloadDatabase(BusySpinner& busySpinner);
        void DownloadComposer(SndhComposer& dbComposer);

    private:
        Array<SongSource> m_songs;
        Array<char> m_strings;
        Array<ArtistSource> m_artists;
        ArtistSource m_availableArtistIds;
        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        struct  
        {
            Array<SndhComposer> composers;
            Array<SndhSong> songs;
            Array<char> strings;
        } m_db;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
    };
}
// namespace rePlayer