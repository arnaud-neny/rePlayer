#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

typedef void CURL;

namespace rePlayer
{
    class SourceZXArt : public Source
    {
    public:
        SourceZXArt();
        ~SourceZXArt() final;

        void FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner) final;
        void FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner) final;
        Import ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        void Load() final;
        void Save() const final;

        void BrowserInit(BrowserContext& context) final;
        void BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter) final;
        int64_t BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const final;
        void BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const final;
        void BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs) final;
        std::string BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const final;
        Array<BrowserSong> BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry);

        static constexpr SourceID::eSourceID kID = SourceID::ZXArtID;

    private:
        struct Chars
        {
            uint32_t offset = 0;

            const char* operator()(const Array<char>& blob) const;

            void Set(Array<char>& blob, const char* otherString);
            void Set(Array<char>& blob, const std::string& otherString);
            template <typename T>
            void Copy(const Array<char>& blob, Array<T>& otherblob) const;
            bool IsSame(const Array<char>& blob, const char* otherString) const;
            bool IsSame(const Array<char>& blob, const std::string otherString) const;
        };

        struct ZxArtArtist
        {
            uint32_t id;
            uint32_t numSongs;
            Chars realName;
            Chars handles;
            uint16_t numHandles = 1;
            uint16_t country = 0;
            uint32_t songs = 0;
        };

        struct ZxArtSong
        {
            uint32_t id;
            Chars name;
            MediaType type;
            uint16_t year;
            uint16_t numArtists;
            uint16_t artists[0];

            static uint32_t Size() { return offsetof(ZxArtSong, artists); }
        };

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

        static constexpr BrowserColumn kColumnArtist = BrowserColumn(1);
        static constexpr BrowserColumn kColumnYear = BrowserColumn(2);
        static constexpr BrowserColumn kColumnId = BrowserColumn(3);
        static constexpr BrowserStage kStageArtists = { BrowserStageId(1), BrowserStageType::Folder };
        static constexpr BrowserStage kStageSongs = { BrowserStageId(2), BrowserStageType::Song };

    public:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        bool GetSongs(SourceResults& collectedSongs, const Array<uint8_t>& buffer, bool isCheckable, CURL* curl, uint32_t& start) const;
        ArtistSheet* GetArtist(uint32_t id, CURL* curl) const;

        bool DownloadDatabase(BusySpinner& busySpinner);
        void DownloadArtist(ZxArtArtist& artist);
        bool GetDbSongs(const Array<uint8_t>& buffer, uint32_t& start);

        void AddSong(const ZxArtSong& dbSong, SourceResults& collectedSongs);

    private:
        struct
        {
            Array<ZxArtArtist> artists;
            Array<uint8_t> songs;
            Array<char> strings;
        } m_db;

        Array<SongSource> m_songs;
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
    };
}
// namespace rePlayer