#include "URLImport.h"

// Core
#include <IO/File.h>

// rePlayer
#include <IO/StreamUrl.h>

namespace rePlayer
{
    const char* const SourceURLImport::ms_filename = MusicPath "URL" MusicExt;

    SourceURLImport::SourceURLImport()
    {
        m_songs.Add({});
        m_strings.Add('\0');
    }

    SourceURLImport::~SourceURLImport()
    {}

    void SourceURLImport::FindArtists(ArtistsCollection& artists, const char* name)
    {
        (void)artists; (void)name;
    }

    void SourceURLImport::ImportArtist(SourceID importedArtistID, SourceResults& results)
    {
        (void)importedArtistID; (void)results;
    }

    void SourceURLImport::FindSongs(const char* name, SourceResults& collectedSongs)
    {
        (void)name; (void)collectedSongs;
    }

    std::pair<SmartPtr<io::Stream>, bool> SourceURLImport::ImportSong(SourceID sourceId, const std::string& path)
    {
        (void)path;
        return { StreamUrl::Create(m_strings.Items(m_songs[sourceId.internalId].url)), false };
    }

    void SourceURLImport::OnArtistUpdate(ArtistSheet* artist)
    {
        (void)artist;
        assert(0 && "this should never be called");
    }

    void SourceURLImport::OnSongUpdate(const Song* const song)
    {
        (void)song;
        assert(0 && "this should never be called");
    }

    void SourceURLImport::DiscardSong(SourceID sourceId, SongID newSongId)
    {
        assert(sourceId.sourceId == kID);
        m_songs[sourceId.internalId].songId = newSongId;
        m_songs[sourceId.internalId].isDiscarded = true;
        m_isDirty = true;
    }

    void SourceURLImport::InvalidateSong(SourceID sourceId, SongID newSongId)
    {
        assert(sourceId.sourceId == kID);
        m_songs[sourceId.internalId].songId = newSongId;
        m_songs[sourceId.internalId].isDiscarded = false;
        m_isDirty = true;
    }

    void SourceURLImport::Load()
    {
        auto file = io::File::OpenForRead(ms_filename);
        if (file.IsValid())
        {
            if (file.Read<uint64_t>() != kVersion)
            {
                assert(0 && "file read error");
                return;
            }
            file.Read<uint32_t>(m_songs);
            file.Read<uint32_t>(m_strings);
        }
    }

    void SourceURLImport::Save() const
    {
        if (m_isDirty)
        {
            if (!m_hasBackup)
            {
                std::string backupFileame = ms_filename;
                backupFileame += ".bak";
                io::File::Copy(ms_filename, backupFileame.c_str());
                m_hasBackup = true;
            }
            auto file = io::File::OpenForWrite(ms_filename);
            if (file.IsValid())
            {
                file.Write(kVersion);
                file.Write<uint32_t>(m_songs);
                file.Write<uint32_t>(m_strings);
                m_isDirty = false;
            }
        }
    }
}
// namespace rePlayer