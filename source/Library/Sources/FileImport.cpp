#include "FileImport.h"

// Core
#include <IO/Stream.h>

namespace rePlayer
{
    void SourceFileImport::FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner)
    {
        UnusedArg(artists, name, busySpinner);
    }

    void SourceFileImport::ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner)
    {
        UnusedArg(importedArtistID, results, busySpinner);
    }

    void SourceFileImport::FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner)
    {
        UnusedArg(name, collectedSongs, busySpinner);
    }

    Source::Import SourceFileImport::ImportSong(SourceID sourceId, const std::string& path)
    {
        UnusedArg(sourceId, path);
        return {};
    }

    void SourceFileImport::OnArtistUpdate(ArtistSheet* artist)
    {
        UnusedArg(artist);
    }

    void SourceFileImport::OnSongUpdate(const Song* const song)
    {
        UnusedArg(song);
    }

    void SourceFileImport::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        UnusedArg(sourceId, newSongId);
    }

    void SourceFileImport::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        UnusedArg(sourceId, newSongId);
    }

    void SourceFileImport::Load()
    {}

    void SourceFileImport::Save() const
    {}
}
// namespace rePlayer