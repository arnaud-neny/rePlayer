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

        struct Pack
        {
            Chars url;
            Chars name;
            uint32_t numArtists;
            uint32_t artists[0];
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

    private:
        SongSource* FindSong(uint32_t pack, const std::string& titleUrl);
        ArtistSource* FindArtist(const std::string& url);

    private:
        struct DB
        {
            Array<std::pair<Chars, Chars>> artists; // url/name
            Array<uint32_t> packs;
            Array<char> data;
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