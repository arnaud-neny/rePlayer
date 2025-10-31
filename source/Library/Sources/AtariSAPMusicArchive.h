#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

typedef void CURL;

namespace rePlayer
{
    class SourceAtariSAPMusicArchive : public Source
    {
    public:
        SourceAtariSAPMusicArchive();

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
        Status Validate(SourceID sourceId, SongID songId) final;
        Status Validate(SourceID sourceId, ArtistID artistId) final;

        void Load() final;
        void Save() const final;

        static constexpr SourceID::eSourceID kID = SourceID::AtariSAPMusicArchiveID;

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

        enum class RootID : uint32_t
        {
            kInvalid = 0,
            kNone,
            kComposers,
            kGames,
            kGroups,
            kMisc,
            kUnknown
        };

        struct ASMARoot
        {
            RootID id : 3 = RootID::kInvalid;
            uint32_t label : 29 = 0;
            const char* Label(const Array<char>& blob) const { return Chars{ label }(blob); }
            bool IsSame(const Array<char>& blob, RootID otherId, const char* otherString) const { return id == otherId && Chars{ label }.IsSame(blob, otherString); }
        };
        struct ASMAArtist
        {
            Chars name;
            uint16_t country = 0;
            uint16_t handleIndex = 0;
            uint32_t numHandles = 0;
            uint32_t songs = 0;
            uint32_t numSongs = 0;
        };
        struct ASMASong
        {
            uint32_t root = 0;
            Chars path;
            Chars name;
            uint16_t date = 0;
            uint16_t numArtists = 0;
            struct Artist
            {
                uint32_t id = 0;
                uint32_t nextSong = 0;
            } artists[0];
        };

        struct SourceRoot
        {
            RootID id : 3 = RootID::kInvalid;
            uint32_t refcount : 29 = 0;
            Chars label;
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
            uint16_t numArtists = 0;
            uint16_t artists[0];
            const char* name() const { return reinterpret_cast<const char*>(artists + numArtists); }
            char* name() { return reinterpret_cast<char*>(artists + numArtists); }

            bool IsValid() const { return songId != SongID::Invalid || isDiscarded == true; }
        };

    private:
        template <typename T = uint32_t>
        SourceSong* GetSongSource(T index) const { return m_data.Items<SourceSong>(m_songs[index]); }

        std::string GetArtistName(const ASMAArtist& dbArtist) const;

        uint16_t FindArtist(const ASMAArtist& dbArtist);
        uint32_t FindSong(const ASMASong& dbSong);
        std::string SetupUrl(CURL* curl, SourceSong* songSource) const;

        bool DownloadDatabase(BusySpinner& busySpinner);
        uint32_t FindDatabaseRoot(std::string& filePath);
        uint32_t FindDatabaseArtist(const std::string& author);
        void FindDatabaseArtists(std::string author, uint32_t songOffset);

        ASMASong& DbSong(uint32_t offset) { return *m_db.data.Items<ASMASong>(offset); }
        SourceSong& SrcSong(uint32_t offset) { return *m_data.Items<SourceSong>(offset); }

    private:
        struct
        {
            Array<ASMARoot> roots;
            Array<Chars> handles;
            Array<ASMAArtist> artists;
            Array<uint32_t> songs;
            Array<uint8_t> data;
            Array<char> strings;
        } m_db;

        Array<uint16_t> m_availableRootIds;
        Array<uint32_t> m_availableSongIds;
        Array<uint16_t> m_availableArtistIds;

        Array<char> m_strings;
        Array<SourceArtist> m_artists;
        Array<SourceRoot> m_roots;
        Array<uint8_t> m_data;
        Array<uint32_t> m_songs; // offset in data

        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        bool m_areDataDirty = false; // only when a song is not registered (to remove holes)
        mutable bool m_isDirty = false; // save
        mutable bool m_hasBackup = false;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
    };
}
// namespace rePlayer