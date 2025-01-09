#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

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
        Import ImportSong(SourceID sourceId, const std::string& path) final;
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
        };

    private:
        SongSource* AddSong(uint32_t id);
        SongSource* FindSong(uint32_t id) const;
        Collector Collect(uint32_t artistID) const;
        bool IsInvalidIndex(const char* buffer, uint32_t size) const;

    private:
        Array<SongSource> m_songs;
        mutable bool m_isDirty = false;
        mutable bool m_hasBackup = false;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
    };
}
// namespace rePlayer