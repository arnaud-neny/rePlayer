#pragma once

#include "Database.h"

namespace rePlayer
{
    template <typename ItemType, typename ItemID>
    struct Database::Set<ItemType, ItemID>::Iterator
    {
        Iterator begin() const
        {
            return *this;
        }

        Iterator end() const
        {
            return { e, e };
        }

        bool operator!=(const Iterator& other) const { return b != other.b; }
        Iterator& operator++() { do { ++b; } while (b < e && *b == nullptr); return *this; }
        ItemType* operator*() { return b->Get(); }

        const SmartPtr<ItemType>* b;
        const SmartPtr<ItemType>* e;
    };

    template <typename ItemType, typename ItemID>
    inline Database::Set<ItemType, ItemID>::Iterator Database::Set<ItemType, ItemID>::Items() const
    {
        if (m_numItems > 0)
        {
            auto b = m_items.begin();
            auto e = m_items.end();
            while (b < e && *b == nullptr)
                ++b;
            return { b, e };
        }
        return { nullptr, nullptr };
    }

    template <typename T>
    inline constexpr Database::Flag::Flag(T flags)
        : value(eFlag(flags))
    {}

    inline Database::Flag& Database::Flag::operator=(const Flag& other)
    {
        value = other.value;
        return *this;
    }
    inline Database::Flag& Database::Flag::operator=(Flag&& other)
    {
        value = std::move(other.value);
        return *this;
    }

    inline void Database::Register(DatabaseSongsUI* ui)
    {
        m_songsUI = ui;
    }

    inline void Database::Register(DatabaseArtistsUI* ui)
    {
        m_artistsUI = ui;
    }

    inline Song* Database::operator[](SongID songId) const
    {
        return m_songs.m_items[uint32_t(songId)];
    }

    inline Song* Database::operator[](SubsongID subsongId) const
    {
        return m_songs.m_items[uint32_t(subsongId.songId)];
    }

    inline Artist* Database::operator[](ArtistID artistId) const
    {
        return m_artists.m_items[uint32_t(artistId)];
    }

    template <typename ID>
    inline bool Database::IsValid(ID id) const
    {
        if constexpr (std::is_same<ID, SongID>::value)
            return uint32_t(id) < m_songs.m_items.NumItems() && m_songs.m_items[uint32_t(id)].IsValid();
        else if constexpr (std::is_same<ID, ArtistID>::value)
            return uint32_t(id) < m_artists.m_items.NumItems() && m_artists.m_items[uint32_t(id)].IsValid();
        else if constexpr (std::is_same<ID, SubsongID>::value)
            return uint32_t(id.songId) < m_songs.m_items.NumItems() && m_songs.m_items[uint32_t(id.songId)].IsValid();
        else
        {
            assert(0);
            return false;
        }
    }

    inline auto Database::Songs() const
    {
        return m_songs.Items();
    }

    inline auto Database::Artists() const
    {
        return m_artists.Items();
    }

    inline uint32_t Database::NumSongs() const
    {
        return m_songs.m_numItems;
    }

    inline uint32_t Database::NumArtists() const
    {
        return m_artists.m_numItems;
    }

    inline uint32_t Database::SongsRevision() const
    {
        return m_songs.m_revison;
    }

    inline uint32_t Database::ArtistsRevision() const
    {
        return m_artists.m_revison;
    }

    inline std::string Database::GetTitle(SubsongID subsongId) const
    {
        return GetTitle(subsongId.songId, subsongId.index);
    }

    inline std::string Database::GetTitleAndArtists(SubsongID subsongId) const
    {
        return GetTitleAndArtists(subsongId.songId, subsongId.index);
    }

    template <typename Predicate>
    inline Song* Database::FindSong(Predicate&& predicate) const
    {
        for (auto* songPtr = m_songs.m_items.Items(), *e = songPtr + m_songs.m_items.NumItems(); songPtr < e; songPtr++)
        {
            if (songPtr->IsValid())
            {
                if (std::forward<Predicate>(predicate)(songPtr->Get()))
                    return songPtr->Get();
            }
        }
        return nullptr;
    }

    inline DatabaseArtistsUI* Database::GetArtistsUI() const
    {
        return m_artistsUI;
    }

    inline DatabaseSongsUI* Database::GetSongsUI() const
    {
        return m_songsUI;
    }

    template <typename ItemID, typename ItemType>
    inline void Database::Update(ItemID id, ItemType* item)
    {
        if constexpr (std::is_same<ItemID, SongID>::value)
            m_songs.m_items[uint32_t(id)] = item;
        else
            m_artists.m_items[uint32_t(id)] = item;
    }
}
// namespace rePlayer