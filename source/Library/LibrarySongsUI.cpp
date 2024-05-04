#include "LibrarySongsUI.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>

#include <Database/Database.h>
#include <Library/Library.h>
#include <Library/LibrarySongMerger.h>
#include <RePlayer/Core.h>
#include <RePlayer/CoreHeader.h>

#include <filesystem>

namespace rePlayer
{
    const char* const Library::SongsUI::ms_path = SongsPath;

    Library::SongsUI::SongsUI(Window& owner)
        : DatabaseSongsUI(DatabaseID::kLibrary, owner)
        , m_songMerger(new SongMerger())
    {}

    Library::SongsUI::~SongsUI()
    {
        delete m_songMerger;
    }

    void Library::SongsUI::OnDisplay()
    {
        DatabaseSongsUI::OnDisplay();
        m_songMerger->Update(*this);
    }

    void Library::SongsUI::OnEndUpdate()
    {
        DatabaseSongsUI::OnEndUpdate();

        if (m_deletedSubsongs.IsNotEmpty())
        {
            for (auto subsongIdToRemove : m_deletedSubsongs)
            {
                SmartPtr<Song> holdSong = m_db[subsongIdToRemove.songId];
                auto song = holdSong->Edit();
                Log::Message("Discard: ID_%06X_%02XL \"[%s]%s\"\n", uint32_t(subsongIdToRemove.songId), uint32_t(subsongIdToRemove.index), song->type.GetExtension(), m_db.GetTitleAndArtists(subsongIdToRemove).c_str());

                uint32_t numSubsongs = song->lastSubsongIndex + 1ul;
                song->subsongs[subsongIdToRemove.index].isDiscarded = true;
                for (uint32_t i = 0, e = numSubsongs; i < e; i++)
                {
                    if (song->subsongs[i].isDiscarded)
                        numSubsongs--;
                }
                if (numSubsongs == 0)
                {
                    auto& library = GetLibrary();
                    auto filename = GetFullpath(holdSong);
                    if (!io::File::Delete(filename.c_str()))
                    {
                        Log::Warning("Can't delete file \"%s\"\n", filename.c_str());
                        m_hasFailedDeletes = true;
                    }

                    for (auto sourceId : song->sourceIds)
                        library.m_sources[sourceId.sourceId]->DiscardSong(sourceId, SongID::Invalid);

                    for (auto artistId : song->artistIds)
                    {
                        auto* artist = library.m_db[artistId]->Edit();
                        if (--artist->numSongs == 0)
                            library.m_db.RemoveArtist(artistId);
                    }

                    m_db.RemoveSong(song->id);

                    m_db.Raise(Database::Flag::kSaveArtists);
                }

                Core::Discard(MusicID(subsongIdToRemove, DatabaseID::kLibrary));
            }
            m_db.Raise(Database::Flag::kSaveSongs);
            m_deletedSubsongs.Clear();
        }
    }

    std::string Library::SongsUI::GetFullpath(Song* song) const
    {
        std::string filename = GetDirectory(song);
        //build song filename
        {
            auto name = std::string(song->GetName());
            io::File::CleanFilename(name.data());
            filename += name;
        }
        filename += " [";
        for (uint16_t i = 0; i < song->NumArtistIds(); i++)
        {
            auto* artist = m_db[song->GetArtistId(i)];
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

    std::string Library::SongsUI::GetDirectory(Artist* artist) const
    {
        std::string directory = ms_path;
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

    Library& Library::SongsUI::GetLibrary()
    {
        return reinterpret_cast<Library&>(m_owner);
    }

    void Library::SongsUI::InvalidateCache()
    {
        m_hasFailedDeletes = true;
    }

    void Library::SongsUI::CleanupCache()
    {
        if (m_hasFailedDeletes && std::filesystem::exists(ms_path))
        {
            Array<std::filesystem::path> directories;
            for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(ms_path))
            {
                auto& filePath = dir_entry.path();
                if (!dir_entry.is_directory())
                {
                    auto filename = filePath.stem().string();
                    bool isLostFile = filename.size() < sizeof("][00000000]");
                    if (!isLostFile)
                    {
                        uint32_t id;
                        if (sscanf_s(filename.c_str() + filename.size() - sizeof("][00000000]") + 1, "][%08X]", &id))
                        {
                            isLostFile = !m_db.IsValid(SongID(id));
                            if (!isLostFile)
                            {
                                auto f = GetFullpath(m_db[SongID(id)]);
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
                        Log::Warning("Cache: deleting \"%s\"\n", filePath.c_str());
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
                    Log::Warning("Cache: deleting \"%s\"\n", filePath.c_str());
                    std::filesystem::remove(filePath);
                }
            }
        }
    }

    void Library::SongsUI::OnAddingArtistToSong(Song* song, ArtistID artistId)
    {
        auto oldFileName = GetFullpath(song);
        song->Edit()->artistIds.Add(artistId);
        auto newFileName = GetFullpath(song);
        io::File::Move(oldFileName.c_str(), newFileName.c_str());
    }

    void Library::SongsUI::OnSelectionContext()
    {
        m_songMerger->MenuItem(*this);
    }

    std::string Library::SongsUI::GetDirectory(Song* song) const
    {
        if (song->NumArtistIds() == 0)
        {
            std::string directory = ms_path;
            return directory + "!/";
        }
        return GetDirectory(m_db[song->GetArtistId(0)]);
    }
}
// namespace rePlayer