#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

namespace rePlayer
{
    class SourceVGMRips : public Source
    {
        struct ArtistsCollector;
        struct ArtistCollector;
        struct PacksCollector;
        struct PackCollector;

    public:
        SourceVGMRips();
        ~SourceVGMRips() final;

        void FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner) final;
        void FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner) final;
        Import ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        std::string GetArtistStub(SourceID artistId) const final;

        void Load() final;
        void Save() const final;

        void BrowserInit(BrowserContext& context) final;
        void BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter) final;
        int64_t BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const final;
        void BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const final;
        void BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs) final;
        std::string BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const final;
        Array<BrowserSong> BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry);

        static constexpr SourceID::eSourceID kID = SourceID::VGMRipsID;

    private:
        struct Chars
        {
            uint32_t offset = 0;

            const char* operator()(const Array<char>& blob) const;

            void Set(Array<char>& blob, const char* otherString);
            void Set(Array<char>& blob, const std::string& otherString);
            template <typename T>
            void Copy(const Array<char>& blob, Array<T>& otherblob) const;
            template <bool isCaseSensitive = true>
            bool IsSame(const Array<char>& blob, const char* otherString) const;
        };

        struct VgmRipsArtist
        {
            Chars url;
            Chars name;
            uint32_t packs : 31;
            uint32_t isComplete : 1;
        };

        struct VgmRipsPack
        {
            Chars url;
            Chars name;
            Chars songsUrl;
            uint32_t songs;
            uint16_t year;
            uint16_t numArtists;
            struct
            {
                uint16_t index;
                uint16_t remap;
                uint32_t nextPack;
            } artists[0];
        };

        struct VgmRipsSong
        {
            Chars url;
            Chars name;
            uint32_t nextSong;
        };

        struct PackSource
        {
            Chars url;
            uint32_t numArtists;
            uint32_t artists[0];
        };

        struct SongSource
        {
            uint32_t id = 0;
            uint32_t crc = 0;
            uint32_t size : 31 = 0;
            uint32_t isDiscarded : 1 = 0;//deleted or merged
            SongID songId = SongID::Invalid;
            uint32_t pack = 0;
            Chars url;

            bool IsValid() const { return songId != SongID::Invalid || isDiscarded == true; }
        };

        struct ArtistSource
        {
            uint32_t id = 0;
            Chars url;
        };

        static constexpr BrowserColumn kColumnArtist = BrowserColumn(1);
        static constexpr BrowserColumn kColumnYear = BrowserColumn(2);
        static constexpr BrowserColumn kColumnId = BrowserColumn(3);
        static constexpr BrowserStage kStageArtists = { BrowserStageId(1), BrowserStageType::Folder };
        static constexpr BrowserStage kStagePacks = { BrowserStageId(2), BrowserStageType::Folder };
        static constexpr BrowserStage kStageSongs = { BrowserStageId(3), BrowserStageType::Song };  

    private:
        SongSource* FindSong(uint32_t pack, const std::string& titleUrl);
        ArtistSource* FindArtist(const std::string& url);

        bool DownloadArtists(BusySpinner& busySpinner);

    private:
        struct DB
        {
            Array<VgmRipsArtist> artists;
            Array<uint32_t> packs;
            Array<char> data;
            bool IsFullPacks = false;
        } m_db;

        Array<SongSource> m_songs;
        Array<char> m_data;
        Array<ArtistSource> m_artists;
        Array<uint32_t> m_availableSongIds;
        Array<uint32_t> m_availableArtistIds;
        bool m_areDataDirty = false;
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
    };
}
// namespace rePlayer