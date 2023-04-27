#pragma once

#include "Artist.h"
#include "Proxy.h"
#include "Subsong.h"
#include "SubsongID.h"
#include "Tags.h"
#include <Helpers/CommandBuffer.h>
#include <Replays/ReplayTypes.h>

#include <Blob/BlobArray.h>
#include <Blob/BlobString.h>

namespace rePlayer
{
    template <Blob::Storage storage = Blob::kIsStatic>
    struct SongData : public Blob
    {
        typedef std::conditional<storage == Blob::kIsStatic, SubsongData<kIsStatic>[1], Array<SubsongData<kIsDynamic>>>::type Subsongs;

        SongID id = SongID::Invalid;                            // offset : 4
        uint32_t fileSize = 0;                                  // offset : 8
        uint32_t fileCrc = 0;                                   // offset : 12
        Tag tags;                                               // offset : 16
        uint16_t lastSubsongIndex = 0;                          // offset : 24
        MediaType type;                                         // offset : 26
        BlobString<storage> name;                               // offset : 28
        BlobArray<ArtistID, storage> artistIds;                 // offset : 30
        BlobArray<SourceID, storage> sourceIds;                 // offset : 32
        BlobArray<CommandBuffer::Command, storage> metadata;    // offset : 34
        uint16_t releaseYear = 0;                               // offset : 36
        uint16_t databaseDay = 0;                               // offset : 38
        Subsongs subsongs;                                      // offset : 40

        SongData();
    };

    template <>
    inline SongData<Blob::kIsStatic>::SongData()
    {}

    template <>
    inline SongData<Blob::kIsDynamic>::SongData()
        : subsongs(1)
    {}

    struct SongSheet : public SongData<Blob::kIsDynamic>
    {
        BlobSerializer Serialize() const;

        // refcount
        void AddRef();
        void Release();
    };

    class Song : public Proxy<Song, SongSheet>, protected SongData<Blob::kIsStatic>
    {
        friend struct Blob;
        friend struct Proxy<Song, SongSheet>;
        //friend struct SongSheet;
    public:
        Song() = default;

        // accessors
        const SongID GetId() const;
        const uint32_t GetFileSize() const;
        const uint32_t GetFileCrc() const;
        const Tag GetTags() const;
        const uint16_t GetReleaseYear() const;
        const uint16_t GetDatabaseDay() const;
        const MediaType GetType() const;
        const bool IsInvalid() const;
        const bool IsUnavailable() const;

        const char* GetName() const;

        const Span<ArtistID> ArtistIds() const;
        const uint16_t NumArtistIds() const;
        const ArtistID GetArtistId(size_t index) const;

        const Span<SourceID> SourceIds() const;
        const uint16_t NumSourceIds() const;
        const SourceID GetSourceId(size_t index) const;
        const char* const GetSourceName(size_t index = 0) const;

        const uint16_t GetLastSubsongIndex() const;
        const uint32_t GetSubsongDurationCs(size_t index) const;
        const uint8_t GetSubsongRating(size_t index) const;
        const SubsongState GetSubsongState(size_t index) const;
        const bool IsSubsongDiscarded(size_t index) const;
        const bool IsSubsongPlayed(size_t index) const;
        const char* GetSubsongName(size_t index) const;

        const Span<CommandBuffer::Command> Metadatas() const;

        // modifer
        void CopyTo(SongSheet* song) const;
        void CopySubsongsTo(SongSheet* song) const;

        static constexpr uint32_t kVersion = 0;

    private:
        Song(Song&&) = delete;
        Song(const Song&) = delete;
        Song& operator=(const Song&) = delete;
        Song& operator=(Song&&) = delete;
    };
}
// namespace rePlayer