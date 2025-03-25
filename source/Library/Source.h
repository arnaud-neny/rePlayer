#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Database/Types/Artist.h>
#include <Database/Types/Song.h>
#include <Database/Types/SourceID.h>
#include <RePlayer/CoreHeader.h>

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    struct SourceResults
    {
        enum StateType
        {
            kNew = 0,
            kMerged,//== kDuplicated
            kDiscarded,
            kOwned,

            kChecked = 1 << 2
        };
        struct State
        {
            State& SetChecked(bool isChecked) { s = static_cast<StateType>(isChecked ? s | kChecked : s & ~kChecked); return *this; }
            State& SetSongStatus(StateType songState) { s = static_cast<StateType>((s & ~3) | songState); return *this; }
            StateType GetSongStatus() const { return static_cast<StateType>(s & 3); }
            bool IsNew() const { return (s & 3) == kNew; }
            bool IsOwned() const { return (s & 3) == kOwned; }
            bool IsChecked() const { return s & kChecked; }

            StateType s{ kNew };
        };

        bool IsSongAvailable(SourceID sourceId) const;
        int32_t GetArtistIndex(SourceID sourceId) const;

        Array<SourceID> importedArtists;
        Array<SmartPtr<ArtistSheet>> artists;
        Array<SmartPtr<SongSheet>> songs;
        Array<State> states;
    };

    inline bool SourceResults::IsSongAvailable(SourceID sourceId) const
    {
        for (auto song : songs)
        {
            if (song->sourceIds[0] == sourceId)
                return true;
        }
        return false;
    }

    inline int32_t SourceResults::GetArtistIndex(SourceID sourceId) const
    {
        for (auto artistIt = artists.begin(); artistIt != artists.end(); artistIt++)
        {
            if ((*artistIt)->sources[0].id == sourceId)
                return static_cast<int32_t>(artistIt - artists.begin());
        }
        return -1;
    }

    class Source
    {
    public:
        struct ArtistsCollection
        {
            struct Artist
            {
                std::string name;
                std::string description;
                SourceID id;
            };
            Array<Artist> matches;
            Array<Artist> alternatives;
        };

        struct Import
        {
            SmartPtr<io::Stream> stream;
            bool isMissing = false;
            bool isArchive = false;
            bool isPackage = false;
            eExtension extension = eExtension::Unknown;
        };

    public:
        virtual ~Source() {}
        virtual void FindArtists(ArtistsCollection& artists, const char* name) = 0;
        virtual void ImportArtist(SourceID importedArtistID, SourceResults& results) = 0;
        virtual void FindSongs(const char* name, SourceResults& collectedSongs) = 0;
        virtual Import ImportSong(SourceID sourceId, const std::string& path) = 0;
        virtual void OnArtistUpdate(ArtistSheet* artist) = 0;
        virtual void OnSongUpdate(const Song* const song) = 0;
        virtual void DiscardSong(SourceID sourceId, SongID newSongId) = 0;
        virtual void InvalidateSong(SourceID sourceId, SongID newSongId) = 0;

        virtual std::string GetArtistStub(SourceID artistId) const { UnusedArg(artistId); return {}; }

        virtual bool IsValidationEnabled() const { return false; }
        virtual Status Validate(SourceID sourceId, SongID songId) { (void)sourceId; (void)songId; return Status::kOk; }
        virtual Status Validate(SourceID sourceId, ArtistID artistId) { (void)sourceId; (void)artistId; return Status::kOk; }

        virtual void Load() = 0;
        virtual void Save() const = 0;
    };
}
// namespace rePlayer