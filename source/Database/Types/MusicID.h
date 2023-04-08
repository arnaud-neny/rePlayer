#pragma once

#include "Song.h"

namespace core
{
    template <typename RefCountClass>
    class SmartPtr;
}
// namespace core

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    class Database;

    enum class PlaylistID : uint32_t
    {
        kInvalid = 0
    };

    enum class DatabaseID : uint32_t
    {
        kLibrary = 0,
        kPlaylist,
        kCount
    };

    struct MusicID
    {
        SubsongID subsongId;
        PlaylistID playlistId : 31 = PlaylistID::kInvalid;
        DatabaseID databaseId : 1 = DatabaseID::kLibrary;

        MusicID() = default;
        MusicID(SubsongID otherSubsongId, DatabaseID otherDatabaseId);

        bool operator==(MusicID other) const;

        uint64_t GetId() const;

        bool IsValid() const;

        Song* GetSong() const;
        Artist* GetArtist(ArtistID artistId) const;

        std::string GetTitle() const;
        std::string GetArtists() const;

        std::string GetFullpath() const;

        void MarkForSave();
        void Track() const;

        // Helpers
        SmartPtr<core::io::Stream> GetStream();

        // ImGui

        void DisplayArtists() const;
        void SongTooltip() const;
    };
}
// namespace rePlayer

#include "MusicID.inl.h"