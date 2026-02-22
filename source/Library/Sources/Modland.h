#pragma once

#include "../Source.h"

#include <Thread/SpinLock.h>

typedef void CURL;

namespace rePlayer
{
    class SourceModland : public Source
    {
    public:
        SourceModland();
        ~SourceModland() final;

        void FindArtists(ArtistsCollection& artists, const char* name, BusySpinner& busySpinner) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results, BusySpinner& busySpinner) final;
        void FindSongs(const char* name, SourceResults& collectedSongs, BusySpinner& busySpinner) final;
        Import ImportSong(SourceID sourceId, const std::string& path) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

        std::string GetArtistStub(SourceID artistId) const final;

        bool IsValidationEnabled() const final;
        Status Validate(SourceID sourceId, SongID songId) final;
        Status Validate(SourceID sourceId, ArtistID artistId) final;

        void Load() final;
        void Save() const final;

        static constexpr SourceID::eSourceID kID = SourceID::ModlandSourceID;

    private:
        struct Chars
        {
            uint32_t offset = 0;

            const char* operator()(const Array<char>& blob) const;

            void Set(Array<char>& blob, const char* otherString);
            void Set(Array<char>& blob, const std::string& otherString);
            template <typename T>
            void Copy(const Array<char>& blob, Array<T>& otherblob) const;
            template <bool isCaseSensitive = true>
            bool IsSame(const Array<char>& blob, const char* otherString) const;
        };

        struct ModlandReplay
        {
            enum Type : int32_t
            {
                kSidPlay = -5,
                kOktalyzer = -4,
                kMusicEditor = -3,
                kOctaMED = -2,
                kSidMon1 = -1,
                kDefault = 0,
                kTFMX,
                kRichardJoseph,
                kAudioSculpture,
                kDirkBialluch,
                kDynamicSynthesizer,
                kInfogrames,
                kJasonPage,
                kKrisHatlelid,
                kMagneticFieldsPacker,
                kMarkCookseyOld,
                kPierreAdanePacker,
                kPokeyNoise,
                kQuartetST,
                kSoundPlayer,
                kSynthDream,
                kSynthPack,
                kThomasHermann,
                kPaulRobotham,
                kTFMXST,
                kStartrekkerAM,
                kUniqueDevelopment,
                kStereoSidplayer,
                kKensAdLibMusic,
                kAdLibTracker,
                kAdLibSierra,
                kAdLibVisualComposer,
                kKensDigitalMusic,
                kMDX,
                kQSF,
                kGSF,
                k2SF,
                kSSF,
                kDSF,
                kPSF,
                kPSF2,
                kUSF,
                kSNSF,
                kMBM,
                kMBMEdit,
                kFACSoundTracker,
                kDelitrackerCustom,
                kIFFSmus,
                kEuphony,
                kSoundSmith,
                kFMP
            };

            Chars name;
            Chars ext;
            Type type = kDefault;
        };

        struct ModlandReplayOverride
        {
            const char* const name;
            ModlandReplay::Type type;
            void (*nextUrlAndName)(CURL*, const ModlandReplayOverride* const, const SourceModland&, std::string&, std::string&);
            MediaType (*getTypeAndName)(std::string&);
            bool (*isIgnored)(const char*);
            bool (*isMulti)(const char*) = nullptr;
            bool isKeepingLink = false;
            uint32_t item = 0;
        };

        struct ModlandArtist
        {
            Chars name;
            uint32_t songs = 0;
            uint32_t numSongs = 0;
        };

        struct ModlandSong
        {
            Chars name;
            uint16_t replayId;
            uint16_t isExtensionOverriden;
            uint16_t artists[2];
            uint32_t item;
            uint32_t nextSong[2]; // next song from each artist
        };

        struct ModlandItem
        {
            Chars name;
            uint32_t next;
        };

        struct SourceReplay
        {
            Chars name;
            Chars ext; // 0 means ext is built by the replay (> ModlandReplay::kDefault)
        };

        struct SourceArtist
        {
            uint32_t refcount = 0;
            Chars name;
        };

        struct SourceSong
        {
            SongID songId = SongID::Invalid;
            uint32_t crc = 0;
            uint32_t size : 30 = 0;
            uint32_t isDiscarded : 1 = 0; // deleted or merged
            uint32_t isExtensionOverriden : 1 = 0; // default replay extension doesn't match

            uint16_t replay = 0;
            uint16_t artists[2] = { 0, 0 };
            char name[0];

            bool IsValid() const { return songId != SongID::Invalid || isDiscarded == true; }
        };

    private:
        SourceSong* GetSongSource(uint32_t index) const;

        uint16_t FindArtist(const char* const name);
        uint32_t FindSong(const ModlandSong& dbSong);
        std::string SetupUrl(CURL* curl, SourceSong* songSource) const;
        static std::string CleanUrl(CURL* curl, const char* url);
        Import ImportMultiSong(SourceID sourceId, const ModlandReplayOverride* const replay, const std::string& path);
        Import ImportPkSong(SourceID sourceId, ModlandReplay::Type replayType, const std::string& path);

        static ModlandReplayOverride* const GetReplayOverride(ModlandReplay::Type type);
        static const ModlandReplayOverride* const GetReplayOverride(const char* name);
        const ModlandReplayOverride* const GetReplayOverride(SourceSong* songSource) const;

        MediaType UpdateMediaType(const ModlandSong& dbSong, std::string& dbSongName) const;

        bool DownloadDatabase(BusySpinner* busySpinner);
        void DecodeDatabase(char* bufBegin, const char* bufEnd, BusySpinner* busySpinner);

        uint16_t FindDatabaseReplay(const char* newReplay);
        uint16_t FindDatabaseArtist(const char* newArtist);

    private:
        struct
        {
            Array<ModlandReplay> replays;
            Array<ModlandArtist> artists;
            Array<ModlandSong> songs;
            Array<ModlandItem> items;
            Array<char> strings;
        } m_db;

        Array<uint32_t> m_availableSongIds;
        Array<uint16_t> m_availableArtistIds;
        Array<uint16_t> m_availableReplayIds;

        Array<char> m_strings;
        Array<SourceReplay> m_replays;
        Array<SourceArtist> m_artists;
        Array<uint8_t> m_data;
        Array<uint32_t> m_songs; // offset in data

        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        bool m_areDataDirty = false; // only when a song is not registered (to remove holes)
        mutable bool m_isDirty = false; // save
        mutable bool m_hasBackup = false;

        thread::SpinLock m_mutex;

        static const char* const ms_filename;
        static ModlandReplayOverride ms_replayOverrides[];
    };
}
// namespace rePlayer