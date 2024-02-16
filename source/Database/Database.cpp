// Core
#include <IO/File.h>
#include <Thread/Thread.h>

// rePlayer
#include <Database/DatabaseSongsUI.h>
#include <Database/Types/Proxy.inl.h>

#include "Database.h"

// stl
#include <atomic>

namespace rePlayer
{
    template <typename ItemType, typename ItemID>
    template <typename OtherItemType>
    ItemType* Database::Set<ItemType, ItemID>::Add(OtherItemType* item)
    {
        std::atomic_ref(m_revision)++;
        ItemID id;
        if (m_availableIds.IsEmpty())
            id = m_items.Push<ItemID>();
        else
            id = m_availableIds.Pop();
        item->id = id;
        m_numItems++;
        auto newItem = ItemType::Create(item);
        m_items[uint32_t(id)] = newItem;
        return newItem;
    }

    template <typename ItemType, typename ItemID>
    void Database::Set<ItemType, ItemID>::Remove(ItemID id)
    {
        std::atomic_ref(m_revision)++;
        assert(m_items[uint32_t(id)].IsValid());
        m_items[uint32_t(id)].Reset();
        m_availableIds.Add(id);
        m_numItems--;
    }

    template <typename ItemType, typename ItemID>
    template <typename OtherItemType>
    ItemType* Database::Set<ItemType, ItemID>::AddFrozen(OtherItemType* item)
    {
        ItemID id;
        m_spinLock.Lock();
        if (m_availableIds.IsEmpty())
            id = ItemID(m_frozenIndex++);
        else
            id = m_availableIds.Pop();
        m_spinLock.Unlock();
        item->id = id;
        auto newItem = ItemType::Create(item);
        newItem->AddRef();
        return newItem;
    }

    template <typename ItemType, typename ItemID>
    Status Database::Set<ItemType, ItemID>::Load(io::File& file)
    {
        m_revision++;
        m_items.Clear();
        m_availableIds.Clear();

        if (file.Read<uint32_t>() != ItemType::kVersion)
        {
            assert(0 && "file read error");
            return Status::kFail;
        }

        m_numItems = file.Read<uint32_t>();

        m_items.Add(nullptr);
        uint32_t nextId = 1;
        for (uint32_t i = 0, n = m_numItems; i < n; i++, nextId++)
        {
            auto item = ItemType::Load(file);
            auto id = static_cast<uint32_t>(item->GetId());
            for (; nextId < id; nextId++)
            {
                m_items.Add(nullptr);
                m_availableIds.Add(static_cast<ItemID>(nextId));
            }
            m_items.Add(item);
        }

        return Status::kOk;
    }

    template <typename ItemType, typename ItemID>
    void Database::Set<ItemType, ItemID>::Save(io::File& file)
    {
        file.Write(ItemType::kVersion);
        file.WriteAs<uint32_t>(m_numItems);
        for (auto& item : m_items)
        {
            if (item.IsValid())
                item->Save(file);
        }
    }

    template <typename ItemType, typename ItemID>
    void Database::Set<ItemType, ItemID>::Reset()
    {
        m_items.Reset();
        m_items.Add(nullptr);
        m_availableIds.Reset();
        m_numItems = 0;
    }

    bool Database::Flag::IsEnabled(Flag flags) const
    {
        return (value & flags.value) == flags.value;
    }

    Database::Database()
    {
        Reset();
    }

    Database::~Database()
    {}

    void Database::Reset()
    {
        m_songs.Reset();
        m_artists.Reset();
        m_flags = Flag::kNone;
    }

    std::string Database::GetTitle(SongID songId, int32_t subsongIndex) const
    {
        auto* song = (*this)[songId];
        std::string title = song->GetName();
        if (subsongIndex >= 0)
        {
            auto subsongName = song->GetSubsongName(subsongIndex);
            if (*subsongName)
            {
                title += " / ";
                title += subsongName;
            }
        }
        return title;
    }

    std::string Database::GetArtists(SongID songId)  const
    {
        auto& db = *this;
        auto* song = db[songId];
        std::string artists;
        for (uint16_t i = 0; i < song->NumArtistIds(); i++)
        {
            auto* artist = db[song->GetArtistId(i)];
            if (i != 0)
                artists += " & ";
            artists += artist->GetHandle();
        }
        return artists;
    }

    std::string Database::GetTitleAndArtists(SongID songId, int32_t subsongIndex) const
    {
        auto* song = (*this)[songId];
        std::string str;
        if (song->NumArtistIds() > 0)
        {
            str = "[";
            str += GetArtists(songId);
            str += "]";
        }
        str += GetTitle(songId, subsongIndex);
        return str;
    }

    std::string Database::GetFullpath(SongID songId) const
    {
        auto& db = *this;
        return m_songsUI->GetFullpath(db[songId]);
    }

    Song* Database::AddSong(SongSheet* song)
    {
        if (m_numFreeze == 0)
        {
            assert(thread::GetCurrentId() == thread::ID::kMain);
            return m_songs.Add(song);
        }
        auto* command = new Command;
        command->type = Command::kAddSong;
        command->song = m_songs.AddFrozen(song);
        command->next = std::atomic_ref(m_commandHead).exchange(command);
        return command->song;
    }

    void Database::RemoveSong(SongID songId)
    {
        if (m_numFreeze == 0)
        {
            assert(thread::GetCurrentId() == thread::ID::kMain);
            return m_songs.Remove(songId);
        }
        auto* command = new Command;
        command->type = Command::kRemoveSong;
        command->songId = songId;
        command->next = std::atomic_ref(m_commandHead).exchange(command);
    }

    Artist* Database::AddArtist(ArtistSheet* artist)
    {
        if (m_numFreeze == 0)
        {
            assert(thread::GetCurrentId() == thread::ID::kMain);
            return m_artists.Add(artist);
        }
        auto* command = new Command;
        command->type = Command::kAddArtist;
        command->artist = m_artists.AddFrozen(artist);
        command->next = std::atomic_ref(m_commandHead).exchange(command);
        return command->artist;
    }

    void Database::RemoveArtist(ArtistID artistId)
    {
        if (m_numFreeze == 0)
        {
            assert(thread::GetCurrentId() == thread::ID::kMain);
            return m_artists.Remove(artistId);
        }
        auto* command = new Command;
        command->type = Command::kRemoveArtist;
        command->artistId = artistId;
        command->next = std::atomic_ref(m_commandHead).exchange(command);
    }

    Status Database::LoadSongs(io::File& file)
    {
        return m_songs.Load(file);
    }

    void Database::SaveSongs(io::File& file)
    {
        m_songs.Save(file);
    }

    Status Database::LoadArtists(io::File& file)
    {
        return m_artists.Load(file);
    }

    void Database::SaveArtists(io::File& file)
    {
        m_artists.Save(file);
    }

    void Database::Raise(Flag flags)
    {
        if (flags.IsEnabled(Flag::kSaveArtists))
        {
            std::atomic_ref(m_songs.m_revision)++;
            std::atomic_ref(m_artists.m_revision)++;
        } else if (flags.IsEnabled(Flag::kSaveSongs))
            std::atomic_ref(m_songs.m_revision)++;
        std::atomic_ref atomicFlags(reinterpret_cast<uint32_t&>(m_flags.value));
        atomicFlags |= flags.value;
    }

    Database::Flag Database::Fetch()
    {
        std::atomic_ref atomicFlags(reinterpret_cast<uint32_t&>(m_flags.value));
        return Flag(atomicFlags.exchange(Flag::kNone));
    }

    void Database::TrackSubsong(SubsongID subsongId)
    {
        m_songsUI->TrackSubsong(subsongId);
    }

    void Database::Freeze()
    {
        assert(thread::GetCurrentId() == thread::ID::kMain);
        if (m_numFreeze++ == 0)
        {
            m_songs.m_frozenIndex = m_songs.m_items.NumItems();
            m_artists.m_frozenIndex = m_artists.m_items.NumItems();
        }
    }

    void Database::UnFreeze()
    {
        assert(thread::GetCurrentId() == thread::ID::kMain);
        if (--m_numFreeze == 0)
        {
            if (m_songs.m_frozenIndex != m_songs.m_items.NumItems())
            {
                m_songs.m_items.Resize(m_songs.m_frozenIndex);
                std::atomic_ref(m_songs.m_revision)++;
            }
            if (m_artists.m_frozenIndex != m_artists.m_frozenIndex)
            {
                m_artists.m_items.Resize(m_artists.m_frozenIndex);
                std::atomic_ref(m_artists.m_revision)++;
            }
            auto* commands = m_commandTail.next;
            while (commands)
            {
                auto* next = commands->next;
                switch (commands->type)
                {
                    case Command::kAddSong:
                        m_songs.m_items[uint32_t(commands->song->GetId())].Attach(commands->song);
                        m_songs.m_numItems++;
                        break;
                    case Command::kRemoveSong:
                        m_songs.Remove(commands->songId);
                        break;
                    case Command::kAddArtist:
                        m_artists.m_items[uint32_t(commands->artist->GetId())].Attach(commands->artist);
                        m_artists.m_numItems++;
                        break;
                    case Command::kRemoveArtist:
                        m_artists.Remove(commands->artistId);
                        break;
                }
                delete commands;
                commands = next;
            }
            m_commandTail.next = nullptr;
            m_commandHead = &m_commandTail;
        }
    }
}
// namespace rePlayer