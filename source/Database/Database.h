#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>

#include "Types/Artist.h"
#include "Types/MusicID.h"
#include "Types/Song.h"

namespace core::io
{
    class File;
}
// namespace core::io

namespace rePlayer
{
    class DatabaseArtistsUI;
    class DatabaseSongsUI;

    class Database
    {
    public:
        Database();
        Database(const Database&) = delete;
        Database(Database&&) = delete;
        ~Database();

        void Register(DatabaseSongsUI* ui);
        void Register(DatabaseArtistsUI* ui);
        void Reset();

        Song* operator[](SongID songId) const;
        Song* operator[](SubsongID subsongId) const;
        Artist* operator[](ArtistID artistId) const;

        template <typename ID>
        bool IsValid(ID id) const;

        auto Songs() const;
        auto Artists() const;

        uint32_t NumSongs() const;
        uint32_t NumArtists() const;

        uint32_t SongsRevision() const;
        uint32_t ArtistsRevision() const;

        std::string GetTitle(SongID songId, int32_t subsongIndex = -1) const;
        std::string GetTitle(SubsongID subsongId) const;
        std::string GetArtists(SongID songId) const;
        std::string GetTitleAndArtists(SongID songId, int32_t subsongIndex = -1) const;
        std::string GetTitleAndArtists(SubsongID subsongId) const;
        std::string GetFullpath(SongID songId) const;

        Song* AddSong(SongSheet* song);
        void RemoveSong(SongID songId);
        template <typename Predicate>
        Song* FindSong(Predicate&& predicate) const;

        Artist* AddArtist(ArtistSheet* artist);
        void RemoveArtist(ArtistID artistId);

        Status LoadSongs(io::File& file);
        void SaveSongs(io::File& file);

        Status LoadArtists(io::File& file);
        void SaveArtists(io::File& file);

        struct Flag
        {
            enum eFlag : uint32_t
            {
                kNone = 0,
                kSaveSongs = 1 << 0,
                kSaveArtists = 1 << 1
            };

            constexpr Flag() = default;
            constexpr Flag(const Flag& flags) = default;
            constexpr Flag(Flag&& flags) = default;
            template <typename T>
            constexpr Flag(T flags);

            Flag& operator=(const Flag& other);
            Flag& operator=(Flag&& other);

            bool IsEnabled(Flag flags) const;

            eFlag value = eFlag::kNone;
        };

        void Raise(Flag flags);
        Flag Fetch();

        void TrackSubsong(SubsongID subsongId);

        DatabaseArtistsUI* GetArtistsUI() const;
        DatabaseSongsUI* GetSongsUI() const;

        template <typename ItemID, typename ItemType>
        void Update(ItemID id, ItemType* item);

    private:
        template <typename ItemType, typename ItemID>
        struct Set
        {
            struct Iterator;

            Iterator Items() const;

            template <typename OtherItemType>
            ItemType* Add(OtherItemType* item);
            void Remove(ItemID id);

            Status Load(io::File& file);
            void Save(io::File& file);

            void Reset();

            Array<SmartPtr<ItemType>> m_items;
            Array<ItemID> m_availableIds;
            uint32_t m_numItems = 0;
            uint32_t m_revison = 0;
        };

    private:
        Set<Song, SongID> m_songs;
        Set<Artist, ArtistID> m_artists;
        Flag m_flags = Flag::kNone;

        DatabaseSongsUI* m_songsUI = nullptr;
        DatabaseArtistsUI* m_artistsUI = nullptr;
    };
}
// namespace rePlayer

#include "Database.inl.h"