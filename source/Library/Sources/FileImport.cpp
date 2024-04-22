#include "FileImport.h"

// Core
#include <IO/Stream.h>

namespace rePlayer
{
    void SourceFileImport::FindArtists(ArtistsCollection& /*artists*/, const char* /*name*/)
    {}

    void SourceFileImport::ImportArtist(SourceID /*importedArtistID*/, SourceResults& /*results*/)
    {}

    void SourceFileImport::FindSongs(const char* /*name*/, SourceResults& /*collectedSongs*/)
    {}

    Source::Import SourceFileImport::ImportSong(SourceID /*sourceId*/, const std::string& /*path*/)
    {
        return {};
    }

    void SourceFileImport::OnArtistUpdate(ArtistSheet* /*artist*/)
    {}

    void SourceFileImport::OnSongUpdate(const Song* const /*song*/)
    {}

    void SourceFileImport::DiscardSong(SourceID /*sourceId*/, SongID /*newSongId*/)
    {}

    void SourceFileImport::InvalidateSong(SourceID /*sourceId*/, SongID /*newSongId*/)
    {}

    void SourceFileImport::Load()
    {}

    void SourceFileImport::Save() const
    {}
}
// namespace rePlayer