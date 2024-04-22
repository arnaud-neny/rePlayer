#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

typedef struct Curl_easy CURL;

namespace rePlayer
{
    class SourceZXArt : public Source
    {
    public:
        SourceZXArt();
        ~SourceZXArt() final;

        void FindArtists(ArtistsCollection& artists, const char* name) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results) final;
        void FindSongs(const char* name, SourceResults& collectedSongs) final;
        Import ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        void Load() final;
        void Save() const final;

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

            static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);
        };

    public:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        void GetSongs(SourceResults& collectedSongs, const Array<uint8_t>& buffer, bool isCheckable, CURL* curl) const;
        ArtistSheet* GetArtist(uint32_t id, CURL* curl) const;

        bool DownloadDatabase();

    private:
        struct
        {
            Array<ZxArtArtist> artists;
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