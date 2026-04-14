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

    enum class TrackMode : uint8_t
    {
        None= 0,
        Song = 1 << 7,
        CurrentArtist = 1 << 0,
        NextArtist = 1 << 1,
        SongAndCurrentArtist = Song | CurrentArtist,
        SongAndNextArtist = Song | NextArtist,
        ArtistMask = CurrentArtist | NextArtist
    };
    inline constexpr TrackMode& operator|=(TrackMode& a, TrackMode b) { a = TrackMode(uint8_t(a) | uint8_t(b)); return a; }
    inline constexpr bool operator<=>(TrackMode a, TrackMode b) { return uint8_t(a) & uint8_t(b); }

    struct MusicID
    {
        SubsongID subsongId;
        union
        {
            uint32_t playlistValue = 0;
            struct
            {
                PlaylistID playlistId : 31;
                DatabaseID databaseId : 1;
            };
        };

        MusicID() = default;
        MusicID(SubsongID otherSubsongId, DatabaseID otherDatabaseId);

        constexpr bool operator==(MusicID other) const;
        constexpr bool operator<(MusicID other) const;

        bool IsValid() const;

        Song* GetSong() const;
        Artist* GetArtist(ArtistID artistId) const;

        std::string GetTitle() const;
        std::string GetArtists() const;

        std::string GetFullpath() const;

        void MarkForSave();
        void Track(TrackMode trackMode) const;

        // Helpers
        SmartPtr<core::io::Stream> GetStream() const;

        // ImGui

        void DisplayArtists() const;
        void SongTooltip() const;
    };
}
// namespace rePlayer

#include "MusicID.inl.h"