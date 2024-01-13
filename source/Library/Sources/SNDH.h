#pragma once

#include "../Source.h"

namespace rePlayer
{
    class SourceSNDH : public Source
    {
        struct Collector;
        struct Search;

    public:
        SourceSNDH();
        ~SourceSNDH() final;

        void FindArtists(ArtistsCollection& artists, const char* name) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results) final;
        void FindSongs(const char* name, SourceResults& collectedSongs) final;
        std::pair<SmartPtr<io::Stream>, bool> ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        void Load() final;
        void Save() const final;

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

            static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);
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

    private:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        uint32_t FindArtist(const std::string& url, const std::string& name);

    private:
        Array<SongSource> m_songs;
        Array<char> m_strings;
        Array<ArtistSource> m_artists;
        ArtistSource m_availableArtistIds;
        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        static const char* const ms_filename;
    };
}
// namespace rePlayer