#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

namespace rePlayer
{
    class SourceTheModArchive : public Source
    {
        struct WebHandlerLogin;
        struct Collector;
        struct SearchArtist;
        struct SearchGuessedArtist;
        struct SearchSong;

    public:
        SourceTheModArchive();
        ~SourceTheModArchive() final;

        void FindArtists(ArtistsCollection& artists, const char* name) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results) final;
        void FindSongs(const char* name, SourceResults& collectedSongs) final;
        Import ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        std::string GetArtistStub(SourceID artistId) const final;

        void Load() final;
        void Save() const final;

        static constexpr SourceID::eSourceID kID = SourceID::TheModArchiveSourceID;

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

        struct GuessedArtistSource
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
        };

    private:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        uint32_t FindGuessedArtist(const std::string& name);

    private:
        Array<SongSource> m_songs;
        Array<char> m_strings;
        Array<GuessedArtistSource> m_guessedArtists;
        GuessedArtistSource m_availableArtistIds;
        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
    };
}
// namespace rePlayer