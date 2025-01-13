#include "LibrarySongsUI.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <IO/File.h>

#include <Library/Library.h>
#include <Library/LibraryDatabase.h>
#include <Library/LibrarySongMerger.h>
#include <RePlayer/Core.h>

#include <filesystem>

namespace rePlayer
{
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
                    auto filename = m_db.GetFullpath(holdSong);
                    if (!io::File::Delete(filename.c_str()))
                    {
                        Log::Warning("Can't delete file \"%s\"\n", filename.c_str());
                        Core::GetLibraryDatabase().InvalidateCache();
                    }

                    for (auto sourceId : song->sourceIds)
                        library.m_sources[sourceId.sourceId]->DiscardSong(sourceId, SongID::Invalid);

                    for (auto artistId : song->artistIds)
                    {
                        auto* artist = Core::GetLibraryDatabase()[artistId]->Edit();
                        if (--artist->numSongs == 0)
                            Core::GetLibraryDatabase().RemoveArtist(artistId);
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

    Library& Library::SongsUI::GetLibrary()
    {
        return reinterpret_cast<Library&>(m_owner);
    }

    void Library::SongsUI::OnAddingArtistToSong(Song* song, ArtistID artistId)
    {
        auto oldFileName = m_db.GetFullpath(song);
        song->Edit()->artistIds.Add(artistId);
        auto newFileName = m_db.GetFullpath(song);
        io::File::Move(oldFileName.c_str(), newFileName.c_str());
    }

    void Library::SongsUI::OnSelectionContext()
    {
        m_songMerger->MenuItem(*this);
    }
}
// namespace rePlayer