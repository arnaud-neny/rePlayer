#pragma once

#include "../Source.h"

namespace rePlayer
{
    class SourceModland : public Source
    {
    public:
        SourceModland();
        ~SourceModland() final;

        void FindArtists(ArtistsCollection& artists, const char* name) final;
        void ImportArtist(SourceID importedArtistID, SourceResults& results) final;
        void FindSongs(const char* name, SourceResults& collectedSongs) final;
        std::pair<Array<uint8_t>, bool> ImportSong(SourceID sourceId) final;
        void OnArtistUpdate(ArtistSheet* artist) final;
        void OnSongUpdate(const Song* const song) final;
        void DiscardSong(SourceID sourceId, SongID newSongId) final;
        void InvalidateSong(SourceID sourceId, SongID newSongId) final;

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
            bool IsSame(const Array<char>& blob, const char* otherString) const;
        };

        struct ModlandReplay
        {
            enum Type : int32_t
            {
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
                kStereoSidplayer,
                kKensAdLibMusic,
                kAdLibTracker,
                kAdLibSierra,
                kAdLibVisualComposer,
                kMDX,
                kQSF,
                kGSF,
                k2SF,
                kSSF,
                kDSF,
                kPSF,
                kPSF2,
                kUSF,
                kDelitrackerCustom
            };

            Chars name;
            Chars ext;
            Type type = kDefault;
        };

        struct ModlandReplayOverride
        {
            const char* const name;
            ModlandReplay::Type type;
            const char* (*getHeaderAndUrl)(std::string&);
            MediaType (*getTypeAndName)(std::string&);
            bool (*isIgnored)(const char*);
            bool (*isMulti)(const char*) = nullptr;
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
            uint32_t nextSong[2]; // next song from the each artist
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
            uint32_t isExtensionOverriden : 1 = 0; // defaut replay extension doesn't match

            uint16_t replay = 0;
            uint16_t artists[2] = { 0, 0 };
            char name[0];

            bool IsValid() const { return songId != SongID::Invalid || isDiscarded == true; }
        };

        static constexpr uint64_t kVersion = uint64_t(kMusicFileStamp) | (0ull << 32);

    private:
        SourceSong* GetSongSource(size_t index) const;

        uint16_t FindArtist(const char* const name);
        uint32_t FindSong(const ModlandSong& dbSong);
        std::string SetupUrl(void* curl, SourceSong* songSource) const;
        std::pair<Array<uint8_t>, bool> ImportTFMXSong(SourceID sourceId);
        std::pair<Array<uint8_t>, bool> ImportMultiSong(SourceID sourceId, const ModlandReplayOverride* const replay);
        std::pair<Array<uint8_t>, bool> ImportPkSong(SourceID sourceId, ModlandReplay::Type replayType);

        static const ModlandReplayOverride* const GetReplayOverride(ModlandReplay::Type type);
        static const ModlandReplayOverride* const GetReplayOverride(const char* name);
        const ModlandReplayOverride* const GetReplayOverride(SourceSong* songSource) const;

        bool DownloadDatabase();
        void DecodeDatabase(char* bufBegin, const char* bufEnd);

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

        Array<char> m_strings;
        Array<SourceReplay> m_replays;
        Array<SourceArtist> m_artists;
        Array<uint8_t> m_data;
        Array<uint32_t> m_songs; // offset in data

        bool m_areStringsDirty = false; // only when a string has been removed (to remove holes)
        bool m_areDataDirty = false; // only when a song is not registered (to remove holes)
        mutable bool m_isDirty = false; // save
        mutable bool m_hasBackup = false;

        static const char* const ms_filename;
        static const ModlandReplayOverride ms_replayOverrides[];
    };
}
// namespace rePlayer