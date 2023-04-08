#pragma once

#include "../Source.h"
#include <RePlayer/CoreHeader.h>

namespace rePlayer
{
    class SourceHighVoltageSIDCollection : public Source
    {
    public:
        SourceHighVoltageSIDCollection();
        ~SourceHighVoltageSIDCollection() final;

        void FindArtists(ArtistsCollection& artists, const char* name) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results) final;
        void FindSongs(const char* name, SourceResults& collectedSongs) final;
        std::pair<Array<uint8_t>, bool> ImportSong(SourceID sourceId) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        void Load() final;
        void Save() const final;

        static constexpr SourceID::eSourceID kID = SourceID::HighVoltageSIDCollectionID;

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
        };

        struct HvscRoot
        {
            Chars name;
            bool hasArtist;
        };

        struct HvscArtist
        {
            Chars name;
            uint32_t songs = 0;
            uint32_t numSongs = 0;
        };

        struct HvscSong
        {
            Chars name;
            uint16_t root;
            uint16_t artist; // only the first artist because the database is not well organized
            uint32_t nextSong; // next song from the artist
        };

        struct SourceRoot
        {
            Chars name;
        };

        struct SourceArtist
        {
            uint32_t refcount = 0;
            Chars name;
        };

        struct SourceSong
        {
            SongID songId = SongID::Invalid;
            uint32_t crc = 0;
            uint32_t size : 31 = 0;
            uint32_t isDiscarded : 1 = 0; // deleted or merged

            uint16_t root = 0;
            uint16_t artist = 0;
            char name[0];

            bool IsValid() const { return songId != SongID::Invalid || isDiscarded == true; }
        };

        static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);

    private:
        SourceSong* GetSongSource(size_t index) const;

        uint16_t FindArtist(const char* const name);
        uint32_t FindSong(const HvscSong& dbSong);
        std::string SetupUrl(void* curl, SourceSong* songSource) const;

        void DownloadDatabase();
        void DecodeDatabase(char* bufBegin, const char* bufEnd);

        uint16_t FindDatabaseRoot(const char* newRoot);
        uint16_t FindDatabaseArtist(const char* newArtist);

    private:
        struct
        {
            Array<HvscRoot> roots;
            Array<HvscArtist> artists;
            Array<HvscSong> songs;
            Array<char> strings;
        } m_db;

        Array<uint32_t> m_availableSongIds;
        Array<uint16_t> m_availableArtistIds;

        Array<char> m_strings;
        Array<SourceRoot> m_roots;
        Array<SourceArtist> m_artists;
        Array<uint8_t> m_data;
        Array<uint32_t> m_songs; // offset in data

        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        bool m_areDataDirty = false; // only when a song is not registered (to remove holes)
        mutable bool m_isDirty = false; // save
        mutable bool m_hasBackup = false;

        static const char* const ms_filename;
    };
}
// namespace rePlayer