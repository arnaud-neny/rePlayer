#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Database/Types/Artist.h>
#include <Database/Types/Song.h>
#include <Database/Types/SourceID.h>
#include <RePlayer/CoreHeader.h>

struct ImGuiTextFilter;

namespace core::io
{
    class Stream;
}
// namespace core::io

namespace rePlayer
{
    class BusySpinner;

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

    struct BrowserContext
    {
        SmartPtr<BusySpinner>& busySpinner;

        SourceID::eSourceID sourceId = SourceID::NumSourceIDs;

        struct Stage
        {
            enum class Id : uint8_t
            {
                Root = 0,
            };
            enum class Type : uint8_t
            {
                None = 0,
                Folder = 1 << 0,
                Song = 1 << 1
            };

            Id id : 6 = { Id::Root };
            Type type : 2 = { Type::None };
            bool operator==(Stage stage) const { return rcCast<uint8_t>(*this) == rcCast<uint8_t>(stage); }
            bool HasArtists() const { return uint8_t(type) & uint8_t(Type::Folder); }
            bool HasSongs() const { return uint8_t(type) & uint8_t(Type::Song); }
            void Next() { id = Id(uint8_t(id) + 1); type = Type::None; }
            void EnableArtist(bool isEnabled) { type = Type(isEnabled ? uint8_t(type) | uint8_t(Type::Folder) : uint8_t(type) & ~uint8_t(Type::Folder)); }
        } stage;
        uint32_t stageDbIndex;

        int32_t numColumns;
        uint32_t disabledColumns;
        const char** columnNames;

        struct Entry
        {
            uint32_t dbIndex : 29;
            uint32_t isSong : 1;
            uint32_t isSelected : 1;
            uint32_t isDiscarded : 1;
            union
            {
                SongID songId;
                ArtistID artistId;
            };

            bool operator==(uint32_t i) const { return dbIndex == i; }
        };
        Array<Entry> entries;

        enum class Column : int
        {
            Name = 0
        };

        void Invalidate() { sourceId = SourceID::NumSourceIDs; }
    };

    struct BrowserSong
    {
        std::string url;
        std::string name;
        Array<std::string> artists;
        MediaType type;
    };

    class Source
    {
    protected:
        using BrowserEntry = BrowserContext::Entry;
        using BrowserStage = BrowserContext::Stage;
        using BrowserStageId = BrowserContext::Stage::Id;
        using BrowserStageType = BrowserContext::Stage::Type;
        using BrowserColumn = BrowserContext::Column;

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
        Source(bool canBrowse) : m_canBrowse(canBrowse) {}
        virtual ~Source() {}
        virtual void FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner) = 0;
        virtual void ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner) = 0;
        virtual void FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner) = 0;
        virtual Import ImportSong(SourceID sourceId, const std::string& path) = 0;
        virtual void OnArtistUpdate(ArtistSheet* artist) = 0;
        virtual void OnSongUpdate(const Song* const song) = 0;
        virtual void DiscardSong(SourceID sourceId, SongID newSongId) = 0;
        virtual void InvalidateSong(SourceID sourceId, SongID newSongId) = 0;

        virtual std::string GetArtistStub(SourceID artistId) const { UnusedArg(artistId); return {}; }

        virtual bool IsValidationEnabled() const { return false; }
        virtual Status Validate(SourceID sourceId, SongID songId) { UnusedArg(sourceId, songId); return Status::kOk; }
        virtual Status Validate(SourceID sourceId, ArtistID artistId) { UnusedArg(sourceId, artistId); return Status::kOk; }

        virtual void Load() = 0;
        virtual void Save() const = 0;

        virtual void BrowserInit(BrowserContext& context) { UnusedArg(context); }
        virtual void BrowserPopulate(BrowserContext& context, const ImGuiTextFilter& filter) { UnusedArg(context, filter); }
        virtual int64_t BrowserCompare(const BrowserContext& context, const BrowserEntry& lEntry, const BrowserEntry& rEntry, BrowserColumn column) const { UnusedArg(context, lEntry, rEntry, column); return 0; }
        virtual void BrowserDisplay(const BrowserContext& context, const BrowserEntry& entry, BrowserColumn column) const { UnusedArg(context, entry, column); }
        virtual void BrowserImport(const BrowserContext& context, const BrowserEntry& entry, SourceResults& collectedSongs) { UnusedArg(context, entry, collectedSongs); }
        virtual std::string BrowserGetStageName(const BrowserEntry& entry, BrowserStage stage) const { UnusedArg(entry, stage); return {}; }
        virtual Array<BrowserSong> BrowserFetchSongs(const BrowserContext& context, const BrowserEntry& entry) { UnusedArg(context, entry); return {}; }

        const bool m_canBrowse;

        static constexpr BrowserColumn kColumnName = BrowserColumn::Name;
        static constexpr BrowserStage kStageRoot = {};
    };
}
// namespace rePlayer