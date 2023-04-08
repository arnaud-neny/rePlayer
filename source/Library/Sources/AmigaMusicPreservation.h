#pragma once

#include "../Source.h"
#include <RePlayer/CoreHeader.h>

namespace rePlayer
{
    class SourceAmigaMusicPreservation : public Source
    {
        struct Collector;
        struct CollectorArtist;
        struct CollectorSong;
        struct SearchArtist;
        struct SearchSong;

    public:
        SourceAmigaMusicPreservation();
        ~SourceAmigaMusicPreservation() final;

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

        static constexpr SourceID::eSourceID kID = SourceID::AmigaMusicPreservationSourceID;

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

    private:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        Collector Collect(uint32_t artistID) const;

    private:
        Array<SongSource> m_songs;
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        static const char* const ms_filename;
    };
}
// namespace rePlayer