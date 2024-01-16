#pragma once

#include "../Source.h"

namespace rePlayer
{
    class SourceURLImport : public Source
    {
    public:
        SourceURLImport();
        ~SourceURLImport() final;

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

    private:
        static constexpr SourceID::eSourceID kID = SourceID::URLImportID;
        static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);

        struct SongSource
        {
            SongID songId = SongID::Invalid;
            uint32_t isDiscarded : 1 = 0;
            uint32_t url : 31 = 0;
        };

    private:
        Array<SongSource> m_songs;
        Array<char> m_strings;

        mutable bool m_isDirty = false; // save
        mutable bool m_hasBackup = false;

        static const char* const ms_filename;
    };
}
// namespace rePlayer