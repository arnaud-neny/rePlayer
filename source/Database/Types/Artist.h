#pragma once

#include "Proxy.h"
#include "SourceID.h"

#include <Blob/BlobArray.h>
#include <Blob/BlobString.h>

namespace core::io
{
    class File;
}
// namespace core::io

namespace rePlayer
{
    using namespace core;

    enum class ArtistID : uint16_t { Invalid = 0 };

    struct ArtistSource
    {
        SourceID id;
        uint32_t dummy = 0;
        time_t timeFetch = 0;

        ArtistSource() = default;
        ArtistSource(SourceID sourceId);

        const char* const GetName() const { return SourceID::sourceNames[id.sourceId]; }
    };

    template <Blob::Storage storage = Blob::kIsStatic>
    struct ArtistData : public Blob
    {
        typedef BlobArray<BlobString<storage>, storage> Handles;
        typedef BlobArray<BlobString<storage>, storage> Groups;

        ArtistID id = ArtistID::Invalid;
        uint16_t numSongs = 0;
        BlobString<storage> realName;
        Handles handles;
        BlobArray<uint16_t, storage> countries;
        Groups groups;
        BlobArray<ArtistSource, storage> sources;

        void Tooltip() const;
    };

    struct ArtistSheet : public ArtistData<Blob::kIsDynamic>
    {
        BlobSerializer Serialize() const;
        void Load(io::File& file);
        void Save(io::File& file) const;

        // refcount
        void AddRef();
        void Release();
    };

    class Artist : public Proxy<Artist, ArtistSheet>, public ArtistData<Blob::kIsStatic>
    {
        friend struct Blob;
        friend struct ArtistSheet;
    public:
        Artist() = default;

        // accessors
        const ArtistID GetId() const;

        const uint16_t NumSongs() const;

        const char* GetRealName() const;

        const uint16_t NumHandles() const;
        const char* GetHandle(size_t index = 0) const;
        bool AreSameHandles(ArtistSheet* artistSheet) const;

        const Span<uint16_t> Countries() const;
        const uint16_t NumCountries() const;
        const uint16_t GetCountry(size_t index) const;

        const uint16_t NumGroups() const;
        const char* GetGroup(size_t index) const;
        bool AreSameGroups(ArtistSheet* artistSheet) const;

        const Span<ArtistSource> Sources() const;
        const uint16_t NumSources() const;
        const ArtistSource& GetSource(size_t index = 0) const;

        // helpers
        void Tooltip() const;

        // modifer
        void CopyTo(ArtistSheet* artist) const;

        void Patch(uint32_t version) { (void)version; }

        static constexpr uint32_t kVersion = 0;

    private:
        Artist(Artist&&) = delete;
        Artist(const Artist&) = delete;
        Artist& operator=(const Artist&) = delete;
        Artist& operator=(Artist&&) = delete;
    };
}
// namespace rePlayer