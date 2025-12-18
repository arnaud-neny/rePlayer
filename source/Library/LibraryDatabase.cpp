// Core
#include <Core/Log.h>
#include <Core/String.h>

// rePlayer
#include <Library/Library.h>
#include <RePlayer/Core.h>
#include <RePlayer/CoreHeader.h>

#include "LibraryDatabase.h"

// stl
#include <filesystem>

namespace rePlayer
{
    LibraryDatabase::~LibraryDatabase()
    {
        CleanupCache();
    }

    std::string LibraryDatabase::GetFullpath(Song* song, Array<Artist*>* artists) const
    {
        std::string filename = GetDirectory(song, artists);
        //build song filename
        {
            auto name = std::string(song->GetName());
            io::File::CleanFilename(name.data());
            filename += name;
        }
        filename += " [";
        for (uint16_t i = 0; i < song->NumArtistIds(); i++)
        {
            Artist* artist;
            if (artists)
            {
                artist = nullptr;
                for (auto* a : *artists)
                {
                    if (a->id == song->GetArtistId(i))
                    {
                        artist = a;
                        break;
                    }
                }
                if (artist == nullptr)
                    artist = (*this)[song->GetArtistId(i)];
            }
            else
                artist = (*this)[song->GetArtistId(i)];
            if (i != 0)
                filename += ",";
            std::string artistName = artist->GetHandle();
            io::File::CleanFilename(artistName.data());
            filename += artistName;
        }
        {
            char str[16];
            sprintf(str, "][%08X]", static_cast<uint32_t>(song->GetId()));//easy way to make the file unique
            filename += str;
        }
        filename += ".";
        filename += song->IsArchive() ? "7z" : song->GetType().GetExtension();
        return filename;
    }

    std::string LibraryDatabase::GetDirectory(Song* song, Array<Artist*>* artists) const
    {
        if (song->NumArtistIds() == 0)
        {
            return SongsPath "!/";
        }
        if (artists)
        {
            for (auto* artist : *artists)
            {
                if (song->GetArtistId(0) == artist->id)
                    return GetDirectory(artist);
            }
        }
        return GetDirectory((*this)[song->GetArtistId(0)]);
    }

    bool LibraryDatabase::AddArtistToSong(Song* song, Artist* artist)
    {
        auto oldFileName = GetFullpath(song);
        bool isAdded = Database::AddArtistToSong(song, artist);
        if (isAdded)
            Move(oldFileName, song, "AddArtistToSong");
        return isAdded;
    }

    void LibraryDatabase::Update()
    {
        if (m_deletedSubsongs.IsNotEmpty())
        {
            for (auto subsongIdToRemove : m_deletedSubsongs)
            {
                SmartPtr<Song> holdSong = (*this)[subsongIdToRemove.songId];
                auto song = holdSong->Edit();
                Log::Message("Discard: ID_%06X_%02XL \"[%s]%s\"\n", uint32_t(subsongIdToRemove.songId), uint32_t(subsongIdToRemove.index), song->type.GetExtension(), GetTitleAndArtists(subsongIdToRemove).c_str());

                uint32_t numSubsongs = song->lastSubsongIndex + 1ul;
                song->subsongs[subsongIdToRemove.index].isDiscarded = true;
                for (uint32_t i = 0, e = numSubsongs; i < e; i++)
                {
                    if (song->subsongs[i].isDiscarded)
                        numSubsongs--;
                }
                if (numSubsongs == 0)
                {
                    Delete(holdSong, "Discard");

                    auto& library = Core::GetLibrary();
                    for (auto sourceId : song->sourceIds)
                        library.m_sources[sourceId.sourceId]->DiscardSong(sourceId, SongID::Invalid);

                    for (auto artistId : song->artistIds)
                    {
                        auto* artist = Core::GetLibraryDatabase()[artistId]->Edit();
                        if (--artist->numSongs == 0)
                            Core::GetLibraryDatabase().RemoveArtist(artistId);
                    }

                    RemoveSong(song->id);

                    Raise(Database::Flag::kSaveArtists);
                }

                Core::Discard(MusicID(subsongIdToRemove, DatabaseID::kLibrary));
            }
            Raise(Database::Flag::kSaveSongs);
            m_deletedSubsongs.Clear();
        }
    }

    void LibraryDatabase::CleanupCache()
    {
        if (m_hasFailedDeletes && std::filesystem::exists(SongsPath))
        {
            Array<std::filesystem::path> directories;
            for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(SongsPath))
            {
                auto& filePath = dir_entry.path();
                if (!dir_entry.is_directory())
                {
                    auto filename = filePath.stem().u8string();
                    bool isLostFile = filename.size() < sizeof("][00000000]");
                    if (!isLostFile)
                    {
                        uint32_t id;
                        if (sscanf_s(reinterpret_cast<const char*>(filename.c_str()) + filename.size() - sizeof("][00000000]") + 1, "][%08X]", &id))
                        {
                            isLostFile = !IsValid(SongID(id));
                            if (!isLostFile)
                            {
                                auto f = GetFullpath((*this)[SongID(id)]);
                                std::filesystem::path songPath = io::File::Convert(f.c_str());
                                if (!std::filesystem::exists(songPath) || !std::filesystem::equivalent(filePath, songPath))
                                    isLostFile = true;
                            }
                        }
                        else
                            isLostFile = true;
                    }
                    if (isLostFile)
                    {
                        Log::Warning("Cache: deleting \"%s\"\n", filePath.u8string().c_str());
                        std::filesystem::remove(filePath);
                    }
                }
                else
                    directories.Add(filePath);
            }
            for (int32_t i = directories.NumItems<int32_t>() - 1; i >= 0; i--)
            {
                auto& filePath = directories[i];
                if (std::filesystem::is_empty(filePath))
                {
                    Log::Warning("Cache: deleting \"%s\"\n", filePath.u8string().c_str());
                    std::filesystem::remove(filePath);
                }
            }
        }
    }

    void LibraryDatabase::DeleteInternal(Song* song, const char* logId) const
    {
        auto filename = GetFullpath(song);
        if (io::File::IsExisting(filename.c_str()))
        {
            if (!io::File::Delete(filename.c_str()))
            {
                Log::Warning("%s: Can't delete file \"%s\" to \"%s\"\n", logId, filename.c_str());
                m_hasFailedDeletes = true;
            }
            else
                Log::Message("%s: \"%s\" deleted\n", logId, filename.c_str());
        }
    }

    void LibraryDatabase::MoveInternal(const char* oldFilename, Song* song, const char* logId) const
    {
        if (io::File::IsExisting(oldFilename))
        {
            auto newFilename = GetFullpath(song);
            if (!io::File::Move(oldFilename, newFilename.c_str()))
            {
                io::File::Copy(oldFilename, newFilename.c_str());
                Log::Warning("%s: Can't move file \"%s\" to \"%s\"\n", logId, oldFilename, newFilename.c_str());
                m_hasFailedDeletes = true;
            }
            else
                Log::Message("%s: \"%s\" moved to \"%s\"\n", logId, oldFilename, newFilename.c_str());
        }
    }

    std::string LibraryDatabase::GetDirectory(Artist* artist) const
    {
        std::string directory = SongsPath;
        //build artist directory
        {
            std::string artistName = artist->GetHandle();
            io::File::CleanFilename(artistName.data());
            auto c = ::tolower(artistName[0]);
            if (c < 'a' || c > 'z')
            {
                directory += "#/";
            }
            else
            {
                directory += static_cast<char>(toupper(c));
                directory += '/';
            }
            directory += artistName;
            char str[16];
            sprintf(str, " [%04X]/", static_cast<uint32_t>(artist->GetId()));//easy way to make the directory unique
            directory += str;
        }
        return directory;
    }
}
// namespace rePlayer