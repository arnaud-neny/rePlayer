#pragma once

#include "../Source.h"

namespace rePlayer
{
    class SourceFileImport : public Source
    {
    public:
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
    };
}
// namespace rePlayer