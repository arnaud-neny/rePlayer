#include "Playlist.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ImGui/imgui_internal.h>
#include <IO/File.h>
#include <IO/StreamFile.h>
#include <Thread/SpinLock.h>
#include <Thread/Thread.h>

// rePlayer
#include <Database/DatabaseArtistsUI.h>
#include <Database/SongEditor.h>
#include <Deck/Deck.h>
#include <Deck/Player.h>
#include <IO/StreamArchive.h>
#include <IO/StreamArchiveRaw.h>
#include <IO/StreamUrl.h>
#include <Library/Library.h>
#include <PlayList/PlaylistDatabase.h>
#include <PlayList/PlaylistDropTarget.h>
#include <PlayList/PlaylistSongsUI.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <Replays/Replay.h>

// TagLib
#include <fileref.h>
#include <mpeg/id3v2/id3v2tag.h>
#include <mpeg/mpegfile.h>
#include <tag.h>
#include <tfilestream.h>
#include <tpropertymap.h>

// zlib
#include <zlib.h>

// stl
#include <algorithm>
#include <filesystem>

// curl
#include <curl/curl.h>

#define IS_FILECRC_ENABLED 0

namespace rePlayer
{
    inline Playlist::Cue::Entry& Playlist::Cue::Entry::operator=(const MusicID& musicId)
    {
        *this = {};
        *static_cast<MusicID*>(this) = musicId;
        return *this;
    }

    inline bool Playlist::Cue::Entry::operator==(PlaylistID other) const
    {
        return playlistId == other;
    }

    inline bool Playlist::Cue::Entry::IsAvailable() const
    {
        return !GetSong()->IsInvalid();
    }

    void Playlist::Summary::Load(io::File& file)
    {
        file.Read(numSubsongs);
        file.Read(currentSong);
        file.Read(name);
    }

    void Playlist::Summary::Save(io::File& file)
    {
        file.Write(numSubsongs);
        file.Write(currentSong);
        file.Write(name);
    }

    struct Playlist::AddFilesContext : public thread::SpinLock
    {
        Array<std::string> files;

        struct Entry
        {
            std::string path;
            SongSheet* song;
        };
        Array<Entry> entries;
        struct EntryToScan
        {
            std::string path;
            PlaylistID playlistId;
            MediaType type;
        };
        Array<EntryToScan> entriesToScan;
        struct EntryToUpdate
        {
            SongSheet* song;
            std::string artist;
            PlaylistID playlistId;
        };
        Array<EntryToUpdate> entriesToUpdate;

        Array<std::string> failedEntries;

        int32_t droppedEntryIndex;
        std::atomic<uint32_t> numEntries = 0;

        bool isAcceptingAll = false;
        bool isUrl = false;
        bool isDone = false;
        bool isCancel = false;
        uint16_t time = 0;

        SongID previousSongId = SongID::Invalid;
        PlaylistID previousPlaylistId = PlaylistID::kInvalid;

        AddFilesContext* next = nullptr;
    };

    const char* const Playlist::ms_fileName = MusicPath "playlists" MusicExt;

    Playlist::Playlist()
        : Window("Playlist", ImGuiWindowFlags_NoCollapse)
        , m_cue{ Core::GetPlaylistDatabase() }
        , m_dropTarget(new DropTarget(*this))
        , m_artists(new DatabaseArtistsUI(DatabaseID::kPlaylist, *this))
        , m_songs(new SongsUI(*this))
    {
        auto file = io::File::OpenForRead(ms_fileName);
        if (file.IsValid())
        {
            auto stamp = file.Read<uint32_t>();
            auto version = file.Read<uint32_t>();
            if (stamp == kMusicFileStamp && version <= Core::GetVersion())
            {
                m_cue.entries.Resize(file.Read<uint32_t>());
                m_oldCurrentEntryIndex = m_currentEntryIndex = file.Read<int32_t>();

                m_playlists.Resize(file.Read<uint32_t>());
                for (auto& summary : m_playlists)
                    summary.Load(file);

                auto status = LoadPlaylist(file, m_cue, version);
                if (status == Status::kOk && m_currentEntryIndex >= 0)
                    m_cue.entries[m_currentEntryIndex].Track();
            }
            else
            {
                assert(0 && "file read error");
            }
        }

        Enable(true);
    }

    Playlist::~Playlist()
    {
        delete m_artists;
        delete m_songs;
    }

    void Playlist::Clear()
    {
        m_cue.db.Reset();
        m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
        m_cue.entries.Clear();
        m_oldCurrentEntryIndex = m_currentEntryIndex = -1;
        m_uniqueIdGenerator = PlaylistID::kInvalid;
        m_lastSelectedEntry = PlaylistID::kInvalid;

        Flush();

        Core::GetDeck().OnNewPlaylist();
    }

    void Playlist::Enqueue(MusicID musicId)
    {
        musicId.playlistId = ++m_uniqueIdGenerator;
        m_cue.entries.Add({ musicId });
        m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
    }

    void Playlist::Discard(MusicID musicId)
    {
        bool isDirty = false;
        for (uint32_t i = 0, e = m_cue.entries.NumItems(); i < e;)
        {
            if (m_cue.entries[i].subsongId == musicId.subsongId && m_cue.entries[i].databaseId == musicId.databaseId)
            {
                if (static_cast<int32_t>(i) < m_currentEntryIndex)
                    m_currentEntryIndex--;

                m_cue.entries.RemoveAt(i);
                e--;
                isDirty = true;
            }
            else
                i++;
        }
        if (m_currentEntryIndex >= m_cue.entries.NumItems<int32_t>())
            m_currentEntryIndex = m_cue.entries.NumItems<int32_t>() - 1;

        if (musicId.databaseId == DatabaseID::kLibrary)
        {
            for (uint32_t playlistIndex = 0; playlistIndex < m_playlists.NumItems(); playlistIndex++)
            {
                auto& summary = m_playlists[playlistIndex];

                auto filename = GetPlaylistFilename(summary.name);
                auto file = io::File::OpenForRead(filename.c_str());
                if (file.IsValid())
                {
                    PlaylistDatabase db;
                    Cue cue(db);

                    cue.entries.Resize(summary.numSubsongs);
                    auto stamp = file.Read<uint32_t>();
                    auto version = file.Read<uint32_t>();
                    if (stamp != kMusicFileStamp
                        || version > Core::GetVersion()
                        || LoadPlaylist(file, cue, version) != Status::kOk)
                        continue;
                    file = io::File(); // close file

                    uint32_t numEntries = 0;
                    for (uint32_t i = 0; i < cue.entries.NumItems(); i++)
                    {
                        if (cue.entries[i].subsongId == musicId.subsongId && cue.entries[i].databaseId == musicId.databaseId)
                            continue;
                        if (i != numEntries)
                            cue.entries[numEntries] = cue.entries[i];
                        numEntries++;
                    }
                    if (numEntries == 0 && db.NumSongs() == 0)
                    {
                        io::File::Delete(filename.c_str());
                        m_playlists.RemoveAt(playlistIndex--);
                        isDirty = true;
                    }
                    else if (numEntries != summary.numSubsongs)
                    {
                        file = io::File::OpenForWrite(filename.c_str());
                        if (file.IsValid())
                        {
                            summary.numSubsongs = numEntries;
                            cue.entries.Resize(numEntries);

                            file.Write(kMusicFileStamp);
                            file.Write(Core::GetVersion());
                            SavePlaylist(file, cue);
                        }
                        else
                        {
                            io::File::Delete(filename.c_str());
                            m_playlists.RemoveAt(playlistIndex--);
                        }
                        isDirty = true;
                    }
                }
                else
                {
                    m_playlists.RemoveAt(playlistIndex--);
                    isDirty = true;
                }
            }
        }
        if (isDirty)
            m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
    }

    SmartPtr<core::io::Stream> Playlist::GetStream(Song* song)
    {
        SmartPtr<io::Stream> stream;
        auto sourceId = song->GetSourceId(0);
        if (sourceId.sourceId == SourceID::URLImportID)
            stream = StreamUrl::Create(m_cue.db.GetPath(sourceId));
        else
            stream = io::StreamFile::Create(m_cue.db.GetPath(sourceId));
        if (stream.IsValid() && song->IsArchive())
        {
            if (song->NumSourceIds() > 1)
            {
                stream = StreamArchive::Create(stream, true);
                if (stream.IsValid())
                    stream = stream->Open(m_cue.db.GetPath(song->GetSourceId(1)));
            }
            else
                stream = StreamArchiveRaw::Create(stream);
        }
        return stream;
    }

    SmartPtr<core::io::Stream> Playlist::GetStream(const MusicID musicId)
    {
        auto* song = m_cue.db[musicId.subsongId];
        return GetStream(song);
    }

    SmartPtr<Player> Playlist::LoadSong(const MusicID musicId)
    {
        SmartPtr<Player> player;
        if (musicId.databaseId == DatabaseID::kPlaylist)
        {
            auto* dbSong = m_cue.db[musicId.subsongId];
            auto* song = dbSong->Edit();
            auto sourceId = song->sourceIds[0];
            auto stream = GetStream(dbSong);
            if (stream.IsValid())
            {
                if (dbSong->GetFileSize() == 0 && IS_FILECRC_ENABLED)
                {
                    auto streamSize = stream->GetSize();
                    song->fileSize = uint32_t(streamSize);
                    auto fileCrc = crc32(0L, Z_NULL, 0);
                    auto* moduleData = core::Alloc<uint8_t>(65536);
                    for (uint64_t s = 0; s < streamSize; s += 65536)
                    {
                        auto readSize = stream->Read(moduleData, 65536);
                        fileCrc = crc32_z(fileCrc, moduleData, size_t(readSize));
                    }
                    core::Free(moduleData);
                    song->fileCrc = fileCrc;
                    song->subsongs[0].isDirty = streamSize != 0;
                }

                auto metadata(song->metadata);
                if (auto* replay = Core::GetReplays().Load(stream, song->metadata.Container(), song->type))
                {
                    auto oldType = song->type;
                    auto type = replay->GetMediaType();
                    bool hasChanged = oldType != type || metadata != song->metadata;
                    song->type = type;
                    uint32_t oldNumSubsongs = song->lastSubsongIndex + 1;
                    auto numSubsongs = replay->GetNumSubsongs();
                    if (numSubsongs != oldNumSubsongs || song->subsongs[0].isDirty || song->subsongs[0].isInvalid || oldType.replay != type.replay)
                    {
                        hasChanged = true;

                        song->fileSize = uint32_t(stream->GetSize());
                        song->subsongs.Resize(numSubsongs);
                        song->lastSubsongIndex = uint16_t(numSubsongs - 1);
                        for (uint16_t i = 0; i < numSubsongs; i++)
                        {
                            replay->SetSubsong(i);

                            auto& subsong = song->subsongs[i];
                            subsong.Clear();
                            subsong.durationCs = replay->GetDurationMs() / 10;
                            subsong.isDirty = false;
                            if (numSubsongs > 1)
                            {
                                subsong.name = replay->GetSubsongTitle();
                                if (subsong.name.IsEmpty())
                                {
                                    char txt[32];
                                    sprintf(txt, "subsong %u of %u", i + 1, numSubsongs);
                                    subsong.name = txt;
                                }
                            }
                        }
                        for (uint16_t i = 0; i < oldNumSubsongs; i++)
                        {
                            if (i == musicId.subsongId.index)
                                continue;
                            for (uint32_t j = 0, e = m_cue.entries.NumItems(); j < e;)
                            {
                                if (m_cue.entries[j].subsongId.songId == song->id && m_cue.entries[j].subsongId.index == j)
                                {
                                    if (static_cast<int32_t>(j) < m_currentEntryIndex)
                                        m_currentEntryIndex--;

                                    m_cue.entries.RemoveAt(j);
                                    e--;
                                }
                                else
                                    j++;
                            }
                            if (m_currentEntryIndex >= m_cue.entries.NumItems<int32_t>())
                                m_currentEntryIndex = m_cue.entries.NumItems<int32_t>() - 1;
                        }
                    }
                    else if (song->subsongs[0].isUnavailable)
                    {
                        song->subsongs[0].isUnavailable = false;
                        hasChanged = true;
                    }
                    if (musicId.subsongId.index < numSubsongs)
                    {
                        player = Player::Create(musicId, song, replay, stream);
                        player->MarkSongAsNew(hasChanged);
                        Log::Message("%s: loaded %06X%02X \"%s\"\n", Core::GetReplays().GetName(song->type.replay), uint32_t(musicId.subsongId.songId), uint32_t(musicId.subsongId.index), m_cue.db.GetPath(sourceId));
                    }
                    else
                    {
                        delete replay;
                        Log::Message("%s: discarded %06X%02X \"%s\"\n", Core::GetReplays().GetName(song->type.replay), uint32_t(musicId.subsongId.songId), uint32_t(musicId.subsongId.index), m_cue.db.GetPath(sourceId));
                    }

                    if (hasChanged)
                        m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                }
                else
                {
                    if (!song->subsongs[0].isInvalid)
                    {
                        song->subsongs[0].isInvalid = true;
                        m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                    }
                    Log::Error("Can't find a replay for \"%s\"\n", m_cue.db.GetPath(sourceId));
                }
            }
            else
            {
                if (!song->subsongs[0].isUnavailable)
                {
                    song->subsongs[0].isUnavailable = true;
                    m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                }
                Log::Error("Can't open file \"%s\"\n", m_cue.db.GetPath(sourceId));
            }
        }
        else
            player = Core::GetLibrary().LoadSong(musicId);
        return player;
    }

    void Playlist::LoadPreviousSong(SmartPtr<Player>& currentPlayer, SmartPtr<Player>& nextPlayer)
    {
        auto isLooping = Core::GetDeck().IsLooping();
        auto numEntries = m_cue.entries.NumItems<int32_t>();
        auto currentEntryIndex = m_currentEntryIndex;
        auto lastEntryIndex = isLooping ? currentEntryIndex - numEntries : 0;

        for (;;)
        {
            auto previousEntryIndex = --currentEntryIndex;
            if (isLooping && previousEntryIndex < 0)
                previousEntryIndex += numEntries;

            if (previousEntryIndex >= 0 && m_cue.entries[previousEntryIndex].IsAvailable())
            {
                currentPlayer = LoadSong(m_cue.entries[previousEntryIndex]);
                if (currentPlayer.IsValid())
                {
                    if (currentPlayer->IsNewSong())
                    {
                        currentPlayer->MarkSongAsNew(false);

                        auto cueEntry = m_cue.entries[previousEntryIndex];
                        auto currentSubsongIndex = cueEntry.subsongId.index;
                        bool nextIsDirty = true;
                        for (uint16_t i = 0; i <= currentPlayer->GetSong()->lastSubsongIndex; i++)
                        {
                            if (i == currentSubsongIndex)
                                continue;
                            cueEntry.playlistId = ++m_uniqueIdGenerator;
                            cueEntry.subsongId.index = i;
                            m_cue.entries.Insert(previousEntryIndex + i, cueEntry);
                            if (nextIsDirty)
                            {
                                nextPlayer = LoadSong(cueEntry);
                                m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                                nextIsDirty = true;
                            }
                        }
                    }

                    m_currentEntryIndex = previousEntryIndex;
                    break;
                }
                else
                {
                    if (numEntries != m_cue.entries.NumItems<int32_t>())
                        return LoadPreviousSong(currentPlayer, nextPlayer);
                }
            }

            if (currentEntryIndex <= lastEntryIndex)
            {
                if (!isLooping)
                    m_currentEntryIndex = 0;
                break;
            }
        }
    }

    SmartPtr<Player> Playlist::LoadCurrentSong()
    {
        SmartPtr<Player> player;
        auto isLooping = Core::GetDeck().IsLooping();
        auto numEntries = m_cue.entries.NumItems<int32_t>();
        auto originalEntryIndex = m_currentEntryIndex;
        auto currentEntryIndex = originalEntryIndex;
        auto lastEntryIndex = isLooping ? currentEntryIndex + numEntries : numEntries - 1;

        if (currentEntryIndex >= 0) for (;;)
        {
            auto entryIndex = currentEntryIndex;
            if (isLooping && entryIndex >= numEntries)
                entryIndex -= numEntries;

            if (entryIndex < numEntries && m_cue.entries[entryIndex].IsAvailable())
            {
                player = LoadSong(m_cue.entries[entryIndex]);
                if (player.IsValid())
                {
                    m_currentEntryIndex = entryIndex;

                    if (player->IsNewSong())
                    {
                        player->MarkSongAsNew(false);

                        auto cueEntry = m_cue.entries[entryIndex];
                        auto currentSubsongIndex = cueEntry.subsongId.index;
                        for (uint16_t i = 0; i <= player->GetSong()->lastSubsongIndex; i++)
                        {
                            if (i == currentSubsongIndex)
                                continue;
                            cueEntry.playlistId = ++m_uniqueIdGenerator;
                            cueEntry.subsongId.index = i;
                            m_cue.entries.Insert(entryIndex + i, cueEntry);
                            m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                        }
                    }
                    break;
                }
                else
                {
                    if (numEntries != m_cue.entries.NumItems<int32_t>())
                        return LoadCurrentSong();
                }
            }

            if (currentEntryIndex == lastEntryIndex)
            {
                if (!isLooping)
                    m_currentEntryIndex = numEntries - 1;
                break;
            }

            currentEntryIndex++;
        }

        return player;
    }

    SmartPtr<Player> Playlist::LoadNextSong(bool isAdvancing)
    {
        SmartPtr<Player> player;
        auto isLooping = Core::GetDeck().IsLooping();
        auto numEntries = m_cue.entries.NumItems<int32_t>();
        auto currentEntryIndex = m_currentEntryIndex;
        auto lastEntryIndex = isLooping ? currentEntryIndex + numEntries : numEntries - 1;
        if (isAdvancing)
        {
            for (;;)
            {
                currentEntryIndex++;
                auto entryIndex = currentEntryIndex % numEntries;
                if (currentEntryIndex <= lastEntryIndex && !m_cue.entries[entryIndex].GetSong()->IsInvalid())
                {
                    m_currentEntryIndex = entryIndex;
                    break;
                }
                if (currentEntryIndex > lastEntryIndex)
                    break;
            }
        }

        for(;;)
        {
            auto nextEntryIndex = currentEntryIndex + 1;
            if (isLooping)
                nextEntryIndex = nextEntryIndex % numEntries;

            if (nextEntryIndex < numEntries && m_cue.entries[nextEntryIndex].IsAvailable())
            {
                player = LoadSong(m_cue.entries[nextEntryIndex]);
                if (player.IsValid())
                {
                    if (player->IsNewSong())
                    {
                        player->MarkSongAsNew(false);

                        auto cueEntry = m_cue.entries[nextEntryIndex];
                        auto currentSubsongIndex = cueEntry.subsongId.index;
                        for (uint16_t i = 0; i <= player->GetSong()->lastSubsongIndex; i++)
                        {
                            if (i == currentSubsongIndex)
                                continue;
                            cueEntry.playlistId = ++m_uniqueIdGenerator;
                            cueEntry.subsongId.index = i;
                            m_cue.entries.Insert(nextEntryIndex + i, cueEntry);
                            m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                        }
                    }

                    break;
                }
                else
                {
                    if (numEntries != m_cue.entries.NumItems<int32_t>())
                        return LoadNextSong(false);
                }
            }

            if (currentEntryIndex == lastEntryIndex)
                break;

            currentEntryIndex++;
        }

        return player;
    }

    void Playlist::ValidateNextSong(SmartPtr<Player>& player)
    {
        if (m_currentEntryIndex < 0)
        {
            player.Reset();
            return;
        }

        auto nextPlaylistId = player.IsValid() ? player->GetId().playlistId : PlaylistID::kInvalid;

        auto isLooping = Core::GetDeck().IsLooping();
        auto numEntries = m_cue.entries.NumItems<int32_t>();
        auto currentEntryIndex = m_currentEntryIndex;
        auto lastEntryIndex = isLooping ? currentEntryIndex + numEntries : numEntries - 1;

        SmartPtr<Player> newPlayer;
        for (;;)
        {
            auto nextEntryIndex = currentEntryIndex + 1;
            if (isLooping)
                nextEntryIndex = nextEntryIndex % numEntries;

            if (nextEntryIndex < numEntries && m_cue.entries[nextEntryIndex].IsAvailable())
            {
                if (nextPlaylistId == m_cue.entries[nextEntryIndex].playlistId)
                    return;
                newPlayer = LoadSong(m_cue.entries[nextEntryIndex]);
                if (newPlayer.IsValid())
                {
                    if (newPlayer->IsNewSong())
                    {
                        newPlayer->MarkSongAsNew(false);

                        auto cueEntry = m_cue.entries[nextEntryIndex];
                        auto currentSubsongIndex = cueEntry.subsongId.index;
                        for (uint16_t i = 0; i <= newPlayer->GetSong()->lastSubsongIndex; i++)
                        {
                            if (i == currentSubsongIndex)
                                continue;
                            cueEntry.playlistId = ++m_uniqueIdGenerator;
                            cueEntry.subsongId.index = i;
                            m_cue.entries.Insert(nextEntryIndex + i, cueEntry);
                            m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                        }
                    }

                    break;
                }
                else
                {
                    if (numEntries != m_cue.entries.NumItems<int32_t>())
                        return ValidateNextSong(player);
                }
            }

            if (currentEntryIndex == lastEntryIndex)
                break;

            currentEntryIndex++;
        }

        player = newPlayer;
    }

    MusicID Playlist::GetCurrentEntry() const
    {
        if (m_currentEntryIndex >= 0)
            return m_cue.entries[m_currentEntryIndex];
        return {};
    }

    void Playlist::FocusCurrentSong()
    {
        m_isCurrentEntryFocus = true;
    }

    void Playlist::UpdateDragDropSource(uint8_t dropId)
    {
        m_dropTarget->UpdateDragDropSource(dropId);
    }

    std::string Playlist::OnGetWindowTitle()
    {
        //TODO: cache this operation and check for a dirty state (song/playlist update?)
        uint64_t duration = 0;
        for (auto& entry : m_cue.entries)
            duration += entry.GetSong()->GetSubsongDurationCs(entry.subsongId.index);
        duration /= 100;
        char title[128];
        if (m_addFilesContext)
        {
            const char wait[] = "|/-\\";
            sprintf(title, "Playlist: %u/%u %s - %u:%02u:%02u %c###Playlist", uint32_t(m_currentEntryIndex + 1), m_cue.entries.NumItems(), m_cue.entries.NumItems() > 1 ? "entries" : "entry", uint32_t(duration / 3600), uint32_t((duration % 3600) / 60), uint32_t(duration % 60), wait[m_addFilesContext->time >> 14]);
            m_addFilesContext->time += uint16_t(11 * ImGui::GetIO().DeltaTime * ((1 << 14) - 1));
        }
        else
            sprintf(title, "Playlist: %u/%u %s - %u:%02u:%02u###Playlist", uint32_t(m_currentEntryIndex + 1), m_cue.entries.NumItems(), m_cue.entries.NumItems() > 1 ? "entries" : "entry", uint32_t(duration / 3600), uint32_t((duration % 3600) / 60), uint32_t(duration % 60));

        ImGui::SetNextWindowPos(ImVec2(16.0f, 188.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(430.0f, 480.0f), ImGuiCond_FirstUseEver);

        return title;
    }

    void Playlist::OnDisplay()
    {
        auto& deck = Core::GetDeck();

        m_dropTarget->UpdateDragDropSource(0);

        ButtonUrl();
        ImGui::SameLine();
        ButtonLoad();
        ImGui::SameLine();
        ButtonSave();
        ImGui::SameLine();
        ButtonClear();
        ImGui::SameLine();
        ButtonSort();

        int32_t draggedIndex = -1;
        MusicID currentRatingId;
        bool isDatabaseDropped = false;

        int32_t numColumns = m_cue.db.NumSongs() == 0 ? 1 : m_openedTab != OpenedTab::kNone ? 3 : 2;
        ImGuiTableFlags resizeFlags = numColumns < 3 ? ImGuiTableFlags_NoBordersInBody : ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("tabs", numColumns, resizeFlags | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
            if (numColumns > 1)
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 0.0f, 1);
            if (numColumns > 2)
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 2);
            ImGui::TableNextColumn();

            constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable;

            enum IDs
            {
                kIndex,
                kTitle,
                kArtists,
                kType,
                kDuration,
                kRating,
                kNumIDs
            };

            // song - artists - type - duration - rating
            if (ImGui::BeginTable("tunes", kNumIDs, flags))
            {
                ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder, 0.0f, kIndex);
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.0f, kTitle);
                ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthStretch, 0.0f, kArtists);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 0.0f, kType);
                ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 0.0f, kDuration);
                ImGui::TableSetupColumn("[%]", ImGuiTableColumnFlags_WidthFixed, 0.0f, kRating);
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                char indexFormat[] = "%01d";
                for (auto count = m_cue.entries.NumItems(); count >= 10; count /= 10)
                    indexFormat[2]++;

                ImGuiListClipper clipper;
                clipper.Begin(m_cue.entries.NumItems<int32_t>());
                while (clipper.Step())
                {
                    for (int32_t rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                    {
                        const auto curEntry = m_cue.entries[rowIdx];
                        const auto subsongId = curEntry.subsongId;
                        ImGui::PushID(uint32_t(curEntry.playlistId));

                        auto* song = curEntry.GetSong();

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();

                        if (rowIdx == m_currentEntryIndex && !deck.IsSolo())
                            deck.DisplayProgressBarInTable();
                        else if (song->IsInvalid())
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowIdx & 1 ? ImVec4(0.20f, 0.10f, 0.10f, 1.0f) : ImVec4(0.20f, 0.10f, 0.10f, 0.93f)));
                        else if (song->IsUnavailable())
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowIdx & 1 ? ImVec4(0.20f, 0.20f, 0.00f, 1.0f) : ImVec4(0.20f, 0.20f, 0.00f, 0.93f)));
                        else if (!song->IsSubsongPlayed(subsongId.index))
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowIdx & 1 ? ImVec4(0.25f, 0.25f, 0.25f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 0.93f)));

                        ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick;
                        if (ImGui::Selectable("##select", curEntry.isSelected, selectable_flags, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastTopHeadersRowHeight)))//TBD: using imgui_internal for row height
                        {
                            Core::GetSongEditor().OnSongSelected(curEntry);

                            if (ImGui::GetIO().KeyShift)
                            {
                                if (m_lastSelectedEntry == PlaylistID::kInvalid)
                                    m_lastSelectedEntry = curEntry.playlistId;
                                if (!ImGui::GetIO().KeyCtrl)
                                {
                                    for (auto& entry : m_cue.entries)
                                        entry.isSelected = false;
                                }
                                auto foundIt = m_cue.entries.Find(m_lastSelectedEntry);
                                assert(foundIt != nullptr);
                                auto currentIt = m_cue.entries.Items(rowIdx);
                                if (currentIt > foundIt)
                                    std::swap(foundIt, currentIt);
                                for (; currentIt <= foundIt; currentIt++)
                                    currentIt->isSelected = true;
                            }
                            else
                            {
                                m_lastSelectedEntry = curEntry.playlistId;
                                if (ImGui::GetIO().KeyCtrl)
                                {
                                    m_cue.entries[rowIdx].isSelected = !curEntry.isSelected;
                                }
                                else
                                {
                                    auto isSelected = curEntry.isSelected;
                                    for (auto& entry : m_cue.entries)
                                        entry.isSelected = false;
                                    m_cue.entries[rowIdx].isSelected = !isSelected;
                                }
                            }

                            if (ImGui::IsMouseDoubleClicked(0))
                            {
                                auto numEntries = m_cue.entries.NumItems<int32_t>();
                                auto currentEntryIndex = rowIdx;
                                auto newEntryIndex = rowIdx;
                                auto isLooping = deck.IsLooping();
                                auto lastEntryIndex = isLooping ? currentEntryIndex + numEntries : numEntries;
                                auto currentPlayerId = curEntry;
                                SmartPtr<Player> player1;
                                for (;;)
                                {
                                    player1 = LoadSong(currentPlayerId);
                                    if (player1.IsInvalid())
                                    {
                                        currentEntryIndex++;
                                        if (currentEntryIndex >= lastEntryIndex)
                                            break;
                                        newEntryIndex = currentEntryIndex % numEntries;
                                        currentPlayerId = m_cue.entries[newEntryIndex];
                                    }
                                    else
                                    {
                                        if (player1->IsNewSong())
                                        {
                                            player1->MarkSongAsNew(false);
                                            auto currentSubsongIndex = curEntry.subsongId.index;
                                            for (uint16_t i = 0; i <= player1->GetSong()->lastSubsongIndex; i++)
                                            {
                                                if (i == currentSubsongIndex)
                                                    continue;
                                                currentPlayerId.playlistId = ++m_uniqueIdGenerator;
                                                currentPlayerId.subsongId.index = i;
                                                m_cue.entries.Insert(newEntryIndex + i, currentPlayerId);
                                                m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                                            }
                                        }

                                        m_currentEntryIndex = newEntryIndex;
                                        auto player2 = LoadNextSong(false);
                                        deck.Play(player1, player2);
                                        break;
                                    }
                                }
                            }
                        }
                        auto selectableMin = ImGui::GetItemRectMin();
                        auto selectableMax = ImGui::GetItemRectMax();
                        if (!m_dropTarget->IsEnabled() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip))
                        {
                            m_cue.entries[rowIdx].isSelected = true;
                            if (m_firstDraggedEntryIndex > m_lastDraggedEntryIndex && draggedIndex < 0)
                                draggedIndex = rowIdx;

                            ImGui::SetDragDropPayload("PLAYLIST", nullptr, 0);
                            ImGui::EndDragDropSource();
                        }
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (m_firstDraggedEntryIndex <= m_lastDraggedEntryIndex)
                                draggedIndex = rowIdx;
                            // simply accept the payload for playlist as it will never work because it's always dropping on itself
                            ImGui::AcceptDragDropPayload("PLAYLIST", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

                            if (ImGui::AcceptDragDropPayload("ExternalDragAndDropPlaylist"))
                            {
                                auto mousePos = ImGui::GetMousePos();
                                auto insertPos = abs(mousePos.y - selectableMin.y) < abs(mousePos.y - selectableMax.y) ? rowIdx : rowIdx + 1;
                                ProcessExternalDragAndDrop(insertPos);
                            }

                            if (auto payload = ImGui::AcceptDragDropPayload("DATABASE"))
                            {
                                auto mousePos = ImGui::GetMousePos();
                                auto insertPos = abs(mousePos.y - selectableMin.y) < abs(mousePos.y - selectableMax.y) ? rowIdx : rowIdx + 1;
                                auto currentPlayingEntry = insertPos <= m_currentEntryIndex ? m_cue.entries[m_currentEntryIndex] : Cue::Entry();
                                auto* selectedSongs = reinterpret_cast<MusicID*>(payload->Data);
                                for (uint32_t i = 0, e = payload->DataSize / sizeof(MusicID); i < e; i++)
                                    m_cue.entries.Insert(insertPos + i, { selectedSongs[i] })->playlistId = ++m_uniqueIdGenerator;
                                if (currentPlayingEntry.subsongId.IsValid())
                                    m_currentEntryIndex = m_cue.entries.Find<int32_t>(currentPlayingEntry.playlistId);
                                m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                                isDatabaseDropped = true;
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::SameLine(0.0f, 0.0f);//no spacing
                        {
                            char label[16];
                            sprintf(label, indexFormat, rowIdx + 1);
                            if (ImGui::Button(label, ImVec2(ImGui::GetColumnWidth(), 0.f)))
                                curEntry.Track();
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(curEntry.GetTitle().data());
                        if (ImGui::IsItemHovered())
                            curEntry.SongTooltip();
                        ImGui::TableNextColumn();
                        curEntry.DisplayArtists();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(song->GetType().GetExtension());
                        ImGui::TableNextColumn();
                        {
                            auto subsongDurationCs = song->GetSubsongDurationCs(subsongId.index);
                            ImGui::Text("%d:%02d", subsongDurationCs / 6000, (subsongDurationCs / 100) % 60);
                        }
                        ImGui::TableNextColumn();
                        int32_t subsongRating = song->GetSubsongRating(subsongId.index);
                        if (subsongRating == 0)
                            ImGui::ProgressBar(0.0f, ImVec2(-1.f, 0.f), "N/A");
                        else
                            ImGui::ProgressBar((subsongRating - 1) * 0.01f);
                        if (ImGui::BeginPopupContextItem("Rating", ImGuiPopupFlags_MouseButtonLeft))
                        {
                            currentRatingId = curEntry;
                            if (m_currentRatingId != currentRatingId)
                                m_defaultRating = subsongRating;
                            char label[8] = "N/A";
                            if (subsongRating > 0)
                                sprintf(label, "%d%%%%", subsongRating - 1);
                            ImGui::SliderInt("##rating", &subsongRating, 0, 101, label, ImGuiSliderFlags_NoInput);
                            song->Edit()->subsongs[subsongId.index].rating = subsongRating;
                            ImGui::EndPopup();
                        }

                        ImGui::PopID();
                    }
                }

                if (m_isCurrentEntryFocus)
                {
                    if (m_currentEntryIndex >= 0)
                    {
                        float item_pos_y = clipper.StartPosY + clipper.ItemsHeight * m_currentEntryIndex;
                        ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetWindowPos().y);
                    }
                    m_isCurrentEntryFocus = false;
                }

                ImGui::EndTable();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (!isDatabaseDropped)
                {
                    //drop at the end of the playlist if only hovering over the table
                    if (auto* payload = ImGui::AcceptDragDropPayload("DATABASE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                    {
                        auto* selectedSongs = reinterpret_cast<MusicID*>(payload->Data);
                        for (uint32_t i = 0, e = payload->DataSize / sizeof(MusicID); i < e; i++)
                            m_cue.entries.Add({ selectedSongs[i] })->playlistId = ++m_uniqueIdGenerator;
                        m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyDown(ImGuiKey_Delete))
            {
                MusicID musicId;
                auto currentEntryIndex = m_currentEntryIndex;
                if (currentEntryIndex >= 0)
                {
                    // pick the first entry that is not selected from the current one
                    for (;;)
                    {
                        if (!m_cue.entries[currentEntryIndex].isSelected)
                        {
                            musicId = m_cue.entries[currentEntryIndex];
                            break;
                        }
                        currentEntryIndex++;
                        if (currentEntryIndex == m_cue.entries.NumItems<int32_t>())
                        {
                            // all gone, so try to pick the first one not selected before the current one
                            currentEntryIndex = m_currentEntryIndex - 1;
                            for (;;)
                            {
                                if (currentEntryIndex < 0)
                                    break;
                                if (!m_cue.entries[currentEntryIndex].isSelected)
                                {
                                    musicId = m_cue.entries[currentEntryIndex];
                                    break;
                                }
                                currentEntryIndex--;
                            }
                            break;
                        }
                    }
                }
                auto count = m_cue.entries.RemoveIf([](auto& entry)
                {
                    return entry.isSelected;
                });
                if (count > 0)
                {
                    m_currentEntryIndex = m_cue.entries.Find<int32_t>(musicId);
                    m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                }
            }

            if (numColumns > 1)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(m_openedTab == OpenedTab::kNone ? "<\n \nS\nO\nN\nG\nS\n \n<###Songs" :
                    m_openedTab == OpenedTab::kSongs ? ">\n \nS\nO\nN\nG\nS\n \n>###Songs" : "S\nO\nN\nG\nS###Songs"
                    , ImVec2(0.0f, ImGui::GetContentRegionAvail().y / 2.0f)))
                    m_openedTab = m_openedTab != OpenedTab::kSongs ? OpenedTab::kSongs : OpenedTab::kNone;
                if (ImGui::Button(m_openedTab == OpenedTab::kNone ? "<\n \nA\nR\nT\nI\nS\nT\nS\n \n<###Artists" :
                    m_openedTab == OpenedTab::kArtists ? ">\n \nA\nR\nT\nI\nS\nT\nS\n \n>###Artists" : "A\nR\nT\nI\nS\nT\nS###Artists"
                    , ImVec2(0.0f, -FLT_MIN)))
                    m_openedTab = m_openedTab != OpenedTab::kArtists ? OpenedTab::kArtists : OpenedTab::kNone;
            }
            if (numColumns > 2)
            {
                ImGui::TableNextColumn();
                m_openedTab == OpenedTab::kSongs ? m_songs->OnDisplay() : m_artists->OnDisplay();
            }

            ImGui::EndTable();
        }

        if (draggedIndex >= 0)
            MoveSelection(draggedIndex);

        //simulate the OnPopupClose for the rating slider
        if (currentRatingId != m_currentRatingId)
        {
            if (m_currentRatingId.subsongId.IsValid() && m_defaultRating != m_currentRatingId.GetSong()->GetSubsongRating(m_currentRatingId.subsongId.index))
                m_currentRatingId.MarkForSave();
            m_currentRatingId = currentRatingId;
        }

        //invalidate the move selection if the drag 'n drop is done
        {
            auto payload = ImGui::GetDragDropPayload();
            if (!payload || memcmp(payload->DataType, "PLAYLIST", 8) != 0)
            {
                if (m_firstDraggedEntryIndex != 0xffFFffFF || m_lastDraggedEntryIndex != 0)
                {
                    m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                    m_firstDraggedEntryIndex = 0xffFFffFF;
                    m_lastDraggedEntryIndex = 0;
                }
            }
        }
    }

    void Playlist::OnEndUpdate()
    {
        if (m_dropTarget->IsDropped())
            ProcessExternalDragAndDrop(m_cue.entries.NumItems<int32_t>());

        UpdateFiles();

        m_songs->OnEndUpdate();
    }

    void Playlist::MoveSelection(uint32_t draggedEntryIndex)
    {
        auto currentPlaylistId = m_currentEntryIndex >= 0 ? m_cue.entries[m_currentEntryIndex].playlistId : PlaylistID::kInvalid;

        auto numEntries = m_cue.entries.NumItems();
        if (m_firstDraggedEntryIndex > m_lastDraggedEntryIndex)
        {
            auto firstSelected = draggedEntryIndex;
            for (auto i = static_cast<int32_t>(firstSelected) - 1; i >= 0; i--)
            {
                if (m_cue.entries[i].isSelected)
                {
                    auto count = firstSelected - i;
                    if (count > 1)
                    {
                        auto holdEntry = m_cue.entries[i];
                        std::memmove(m_cue.entries.Items(i), m_cue.entries.Items(i + 1), sizeof(Cue::Entry) * (count - 1));
                        m_cue.entries[firstSelected - 1] = holdEntry;
                    }
                    firstSelected--;
                }
            }
            auto lastSelected = firstSelected;
            for (auto i = lastSelected + 1; i < numEntries; i++)
            {
                if (m_cue.entries[i].isSelected)
                {
                    auto count = i - lastSelected;
                    if (count > 1)
                    {
                        auto holdEntry = m_cue.entries[i];
                        std::memmove(m_cue.entries.Items(lastSelected + 2), m_cue.entries.Items(lastSelected + 1), sizeof(Cue::Entry) * (count - 1));
                        m_cue.entries[lastSelected + 1] = holdEntry;
                    }
                    lastSelected++;
                }
            }
            m_firstDraggedEntryIndex = firstSelected;
            m_lastDraggedEntryIndex = lastSelected;
            m_draggedEntryIndex = draggedEntryIndex;
            if (currentPlaylistId != PlaylistID::kInvalid)
                m_currentEntryIndex = m_cue.entries.Find<int32_t>(currentPlaylistId);
        }
        else if (m_draggedEntryIndex < draggedEntryIndex && m_lastDraggedEntryIndex < numEntries - 1)
        {
            uint32_t dt = draggedEntryIndex - m_draggedEntryIndex;
            if (dt > numEntries - m_lastDraggedEntryIndex - 1)
                dt = numEntries - m_lastDraggedEntryIndex - 1;

            auto* holdEntries = reinterpret_cast<Cue::Entry*>(_alloca(dt * sizeof(Cue::Entry)));
            for (uint32_t i = 0; i < dt; i++)
                holdEntries[i] = m_cue.entries[m_lastDraggedEntryIndex + i + 1];
            std::memmove(m_cue.entries.Items(m_firstDraggedEntryIndex + dt), m_cue.entries.Items(m_firstDraggedEntryIndex), (m_lastDraggedEntryIndex - m_firstDraggedEntryIndex + 1) * sizeof(Cue::Entry));
            for (uint32_t i = 0; i < dt; i++)
                m_cue.entries[m_firstDraggedEntryIndex + i] = holdEntries[i];

            m_firstDraggedEntryIndex += dt;
            m_lastDraggedEntryIndex += dt;
            m_draggedEntryIndex = draggedEntryIndex;
            if (currentPlaylistId != PlaylistID::kInvalid)
                m_currentEntryIndex = m_cue.entries.Find<uint32_t>(currentPlaylistId);
        }
        else if (m_draggedEntryIndex > draggedEntryIndex && m_firstDraggedEntryIndex > 0)
        {
            uint32_t dt = m_draggedEntryIndex - draggedEntryIndex;
            if (dt > m_firstDraggedEntryIndex)
                dt = m_firstDraggedEntryIndex;

            auto* holdEntries = reinterpret_cast<Cue::Entry*>(_alloca(dt * sizeof(Cue::Entry)));
            for (uint32_t i = 0; i < dt; i++)
                holdEntries[i] = m_cue.entries[m_firstDraggedEntryIndex - dt + i];
            std::memmove(m_cue.entries.Items(m_firstDraggedEntryIndex - dt), m_cue.entries.Items(m_firstDraggedEntryIndex), (m_lastDraggedEntryIndex - m_firstDraggedEntryIndex + 1) * sizeof(Cue::Entry));
            for (uint32_t i = 0; i < dt; i++)
                m_cue.entries[m_lastDraggedEntryIndex - dt + i + 1] = holdEntries[i];

            m_firstDraggedEntryIndex -= dt;
            m_lastDraggedEntryIndex -= dt;
            m_draggedEntryIndex = draggedEntryIndex;
            if (currentPlaylistId != PlaylistID::kInvalid)
                m_currentEntryIndex = m_cue.entries.Find<uint32_t>(currentPlaylistId);
        }
    }

    void Playlist::ProcessExternalDragAndDrop(int32_t droppedEntryIndex)
    {
        if (m_dropTarget->IsOverriding())
            Clear();

        AddFiles(droppedEntryIndex, m_dropTarget->AcquireFiles(), m_dropTarget->IsAcceptingAll(), m_dropTarget->IsUrl());
    }

    Status Playlist::LoadPlaylist(io::File& file, Cue& cue, uint32_t version)
    {
        for (uint32_t i = 0, e = cue.entries.NumItems(); i < e; i++)
        {
            SubsongID subsongId;
            if (version == 0)
            {
                union
                {
                    uint32_t value = 0;
                    struct
                    {
                        uint32_t index : 8;
                        SongID songId : 24;
                    };
                } oldSubsongId;
                file.Read(oldSubsongId.value);
                subsongId.index = oldSubsongId.index;
                subsongId.songId = oldSubsongId.songId;
            }
            else if (version < 3)
            {
                file.Read(subsongId.songId);
                subsongId.index = uint16_t(file.Read<uint32_t>());
            }
            else
            {
                file.Read(subsongId.songId);
                file.Read(subsongId.index);
            }
            cue.entries[i] = {};
            cue.entries[i].subsongId = subsongId;
            cue.entries[i].playlistId = ++m_uniqueIdGenerator;
        }

        for (uint32_t i = 0, e = cue.entries.NumItems(); i < e;)
        {
            uint8_t isPlaylistDatabase = 0;
            file.Read(isPlaylistDatabase);
            for (uint32_t j = 0; j < 8 && i < e; j++, i++, isPlaylistDatabase >>= 1)
                cue.entries[i].databaseId = DatabaseID(isPlaylistDatabase & 1);
        }

        auto status = cue.db.Load(file, version);
        if (status == Status::kFail)
            cue.entries.Clear();

        return status;
    }

    void Playlist::SavePlaylist(io::File& file, const Cue& cue)
    {
        for (auto& entry : cue.entries)
        {
            file.Write(entry.subsongId.songId);
            file.Write(entry.subsongId.index);
        }

        for (uint32_t i = 0, e = cue.entries.NumItems(); i < e;)
        {
            uint8_t isPlaylistDatabase = 0;
            for (uint32_t j = 0; j < 8; j++, i++)
            {
                isPlaylistDatabase >>= 1;
                if (i < e && cue.entries[i].databaseId == DatabaseID::kPlaylist)
                    isPlaylistDatabase |= 0x80;
            }
            file.Write(isPlaylistDatabase);
        }

        cue.db.Save(file);
    }

    void Playlist::ButtonUrl()
    {
        if (ImGui::Button("Url"))
            ImGui::OpenPopup("UrlPopup");
        if (ImGui::BeginPopup("UrlPopup"))
        {
            ImGui::Text("Enter the list of URLs to import");
            if (ImGui::InputTextMultiline("##url", &m_inputUrls, ImVec2(-FLT_MIN, 0.0f)))
            {
                m_urls.Clear();
                if (!m_inputUrls.empty())
                {
                    std::string::size_type cpos = 0;
                    do
                    {
                        auto npos = m_inputUrls.find_first_of("\r\n", cpos);
                        if (cpos != npos)
                            m_urls.Add(m_inputUrls.substr(cpos, npos - cpos));
                        if (npos != std::string::npos)
                            cpos = m_inputUrls.find_first_not_of("\r\n", npos);
                        else
                            break;
                    } while (cpos != std::string::npos);
                }
            }
            ImGui::BeginDisabled(m_urls.IsEmpty());
            if (ImGui::Button("Add to playlist"))
            {
                AddFiles(m_cue.entries.NumItems(), m_urls, true, true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    }

    void Playlist::ButtonLoad()
    {
        ImGui::BeginDisabled(m_playlists.IsEmpty());
        if (ImGui::Button("Load"))
            ImGui::OpenPopup("LoadPlaylist");
        if (ImGui::BeginPopup("LoadPlaylist", ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::BeginTable("LoadPlaylistTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 0.0f, 0);
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 0.0f, 1);
                for (auto& summary : m_playlists)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(summary.name.data(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                    {
                        auto file = io::File::OpenForRead(GetPlaylistFilename(summary.name).c_str());
                        if (file.IsValid())
                        {
                            m_oldCurrentEntryIndex = m_currentEntryIndex = summary.currentSong;
                            m_cue.entries.Resize(summary.numSubsongs);
                            m_uniqueIdGenerator = PlaylistID::kInvalid;
                            m_lastSelectedEntry = PlaylistID::kInvalid;
                            m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);

                            auto stamp = file.Read<uint32_t>();
                            auto version = file.Read<uint32_t>();
                            if (stamp == kMusicFileStamp && version <= Core::GetVersion())
                                LoadPlaylist(file, m_cue, version);
                            else
                                assert(0 && "file read error");

                            Flush();

                            Core::GetDeck().OnNewPlaylist();
                        }

                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::TableNextColumn();
                    if (summary.numSubsongs > 1)
                        ImGui::Text("%d songs", summary.numSubsongs);
                    else
                        ImGui::Text("%d song", summary.numSubsongs);
                }
                ImGui::EndTable();
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

    void Playlist::ButtonSave()
    {
        ImGui::BeginDisabled(m_cue.entries.IsEmpty() && m_cue.db.NumSongs() == 0);
        if (ImGui::Button("Save"))
            ImGui::OpenPopup("Save Playlist");
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Save Playlist", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Name", &m_playlistName);
            auto summary = std::find_if(m_playlists.begin(), m_playlists.end(), [this](auto& summary)
            {
                return _stricmp(summary.name.c_str(), m_playlistName.c_str()) == 0;
            });
            if (ImGui::Button(summary != m_playlists.end() ? "Overwrite###Save" : "Save"))
            {
                if (summary == m_playlists.end())
                {
                    m_playlists.Push();
                    summary = &m_playlists.Last();
                }
                summary->name = m_playlistName.c_str();
                summary->numSubsongs = m_cue.entries.NumItems();
                summary->currentSong = m_currentEntryIndex;

                m_cue.db.Fetch();
                SavePlaylistsToc();

                auto file = io::File::OpenForWrite(GetPlaylistFilename(summary->name).c_str());
                if (file.IsValid())
                {
                    file.Write(kMusicFileStamp);
                    file.Write(Core::GetVersion());
                    SavePlaylist(file, m_cue);
                }

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

    void Playlist::ButtonClear()
    {
        ImGui::BeginDisabled(m_cue.entries.IsEmpty() && m_cue.db.NumSongs() == 0);
        if (ImGui::Button("Clear"))
            Clear();
        ImGui::EndDisabled();
    }

    void Playlist::ButtonSort()
    {
        ImGui::BeginDisabled(m_cue.entries.IsEmpty());
        if (ImGui::Button("Sort"))
            ImGui::OpenPopup("SortPopup");
        if (ImGui::BeginPopup("SortPopup"))
        {
            const char* sortModes[] = { "Title", "Artists", "Duration", "Type", "Rating", "Random", "Invert" };
            for (int32_t i = 0; i < IM_ARRAYSIZE(sortModes); i++)
            {
                if (ImGui::Selectable(sortModes[i]))
                {
                    MusicID currentPlayerId;
                    if (m_currentEntryIndex >= 0)
                        currentPlayerId = m_cue.entries[m_currentEntryIndex];
                    if (i == 5)
                    {
                        for (uint32_t entryIdx = ImGui::GetIO().KeyCtrl ? m_currentEntryIndex + 1 : 0, count = m_cue.entries.NumItems(); entryIdx < count; entryIdx++)
                            std::swap(m_cue.entries[entryIdx], m_cue.entries[entryIdx + std::rand() % (count - entryIdx)]);
                    }
                    else if (i == 6)
                    {
                        for (int32_t n = ImGui::GetIO().KeyCtrl ? m_currentEntryIndex + 1 : 0, j = m_cue.entries.NumItems<int32_t>() - 1; n < j; n++, j--)
                            std::swap(m_cue.entries[n], m_cue.entries[j]);
                    }
                    else
                    {
                        static constexpr uint32_t sortTables[5][5] = {
                            {0, 1, 2, 3, 4},
                            {1, 0, 2, 3, 4},
                            {2, 1, 0, 3, 4},
                            {3, 1, 0, 2, 4},
                            {4, 1, 0, 2, 3}
                        };
                        std::sort(m_cue.entries.begin() + (ImGui::GetIO().KeyCtrl ? m_currentEntryIndex + 1 : 0), m_cue.entries.end(), [this, i](const MusicID& l, const MusicID& r)
                        {
                            for (uint32_t sortIdx = 0; sortIdx < 6; sortIdx++)
                            {
                                int32_t d{ 0 };
                                switch (sortTables[i][sortIdx])
                                {
                                case 0:
                                    d = _stricmp(l.GetTitle().data(), r.GetTitle().data());
                                    break;
                                case 1:
                                    d = _stricmp(l.GetArtists().data(), r.GetArtists().data());
                                    break;
                                case 2:
                                    d = (l.GetSong()->GetSubsongDurationCs(l.subsongId.index) / 100) - (r.GetSong()->GetSubsongDurationCs(r.subsongId.index) / 100);
                                    break;
                                case 3:
                                    d = _stricmp(l.GetSong()->GetType().GetExtension(), r.GetSong()->GetType().GetExtension());
                                    break;
                                case 4:
                                    d = l.GetSong()->GetSubsongRating(l.subsongId.index) - r.GetSong()->GetSubsongRating(r.subsongId.index);
                                    break;
                                }
                                if (d != 0)
                                    return d < 0;
                            }
                            return l < r;
                        });
                    }
                    m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                    if (currentPlayerId.subsongId.IsValid())
                        m_currentEntryIndex = m_cue.entries.Find<int32_t>(currentPlayerId);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

    void Playlist::AddFiles(int32_t droppedEntryIndex, const Array<std::string>& files, bool isAcceptingAll, bool isUrl)
    {
        // we're going async for that: do the hard work in the job (identify the file, replay loading, tag), then add the processed files in the playlist (fast work)
        auto* addFilesContext = new AddFilesContext;
        addFilesContext->files = files;
        addFilesContext->droppedEntryIndex = droppedEntryIndex;
        addFilesContext->isAcceptingAll = isAcceptingAll;
        addFilesContext->isUrl = isUrl;
        addFilesContext->next = m_addFilesContext;
        m_addFilesContext = addFilesContext;

        Core::AddJob([this, addFilesContext]()
        {
            auto& replays = Core::GetReplays();
            auto databaseDay = uint16_t((std::chrono::sys_days(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())) - std::chrono::sys_days(std::chrono::days(Core::kReferenceDate))).count());

            for (auto& newFile : addFilesContext->files)
            {
                auto addFile = [&](const std::filesystem::path& path)
                {
                    // find the extension (or prefixes)
                    auto guessedExtension = path.has_extension() ? path.extension().u8string().substr(1) : std::u8string();
                    const auto pathExtension = guessedExtension;
                    const auto pathStem = path.stem().u8string();
                    {
                        std::u8string prefix;
                        for (uint16_t i = 0;; i++)
                        {
                            if (i == uint16_t(eExtension::Count))
                            {
                                if (!prefix.empty())
                                    guessedExtension = prefix;
                                break;
                            }
                            if (_stricmp(reinterpret_cast<const char*>(guessedExtension.c_str()), MediaType::extensionNames[i]) == 0)
                                break;
                            if (prefix.empty() && _strnicmp(MediaType::extensionNames[i], reinterpret_cast<const char*>(pathStem.c_str()), MediaType::extensionLengths[i]) == 0)
                            {
                                if (MediaType::extensionLengths[i] == pathStem.size() || pathStem[MediaType::extensionLengths[i]] == '.')
                                    prefix = reinterpret_cast<const char8_t*>(MediaType::extensionNames[i]);
                            }
                        }
                    }

                    MediaType type = replays.Find(reinterpret_cast<const char*>(guessedExtension.c_str()));
                    if (!addFilesContext->isAcceptingAll && type.value == 0 && !addFilesContext->isUrl)
                    {
                        addFilesContext->Lock();
                        addFilesContext->failedEntries.Add(reinterpret_cast<const char*>(path.u8string().c_str()));
                        addFilesContext->Unlock();
                        return;
                    }
                    addFilesContext->numEntries++;

                    auto* songSheet = new SongSheet;
                    songSheet->type = type;
                    if (type.ext == eExtension::Unknown)
                        songSheet->name = reinterpret_cast<const char*>(path.filename().u8string().c_str());
                    else if (_stricmp(reinterpret_cast<const char*>(pathExtension.c_str()), type.GetExtension()) == 0)
                        songSheet->name = reinterpret_cast<const char*>(pathStem.c_str());
                    else
                    {
                        auto name = path.filename().u8string();
                        auto extSize = type.extensionLengths[size_t(type.ext)];
                        if (_strnicmp(reinterpret_cast<const char*>(name.c_str()), type.GetExtension(), extSize) == 0 && name.size() > extSize && name.c_str()[extSize] == '.')
                            songSheet->name = reinterpret_cast<const char*>(name.c_str() + extSize + 1);
                        else
                            songSheet->name = reinterpret_cast<const char*>(name.c_str());
                    }
                    if (addFilesContext->isUrl)
                    {
                        auto* curl = curl_easy_init();

                        int nameSize = 0;
                        if (auto* name = curl_easy_unescape(curl, songSheet->name.String().c_str(), int(songSheet->name.String().size()), &nameSize))
                        {
                            songSheet->name = name;
                            curl_free(name);
                        }

                        curl_easy_cleanup(curl);
                    }

                    songSheet->databaseDay = databaseDay;
                    songSheet->subsongs.Resize(1);

                    addFilesContext->Lock();
                    addFilesContext->entries.Add({ reinterpret_cast<const char*>(path.u8string().c_str()), songSheet });
                    addFilesContext->Unlock();
                };

                auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(newFile.c_str()));
                if (addFilesContext->isUrl)
                {
                    assert(strcmp((const char*)path.u8string().c_str(), newFile.c_str()) == 0);
                    addFile(path);
                }
                else
                {
                    std::error_code ec;
                    if (std::filesystem::is_directory(path, ec))
                    {
                        for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(path, ec))
                        {
                            if (dir_entry.is_regular_file(ec))
                                addFile(dir_entry.path());
                            if (addFilesContext->isCancel)
                                break;
                        }
                    }
                    else if (std::filesystem::is_regular_file(path, ec))
                        addFile(path);
                }
                if (addFilesContext->isCancel)
                    break;
            }
            while (addFilesContext->numEntries.load() && !addFilesContext->isCancel)
            {
                addFilesContext->Lock();
                auto entriesToScan = std::move(addFilesContext->entriesToScan);
                addFilesContext->Unlock();
                if (entriesToScan.IsEmpty())
                    thread::Sleep(1);

                for (auto& entry : entriesToScan)
                {
                    if (addFilesContext->isCancel)
                        break;

                    addFilesContext->numEntries--;
                    SmartPtr<io::Stream> stream;
                    if (addFilesContext->isUrl)
                        stream = StreamUrl::Create(entry.path);
                    else
                        stream = io::StreamFile::Create(entry.path);
                    if (stream.IsInvalid())
                    {
                        addFilesContext->Lock();
                        addFilesContext->entriesToUpdate.Add({ nullptr, "!", entry.playlistId});
                        addFilesContext->Unlock();
                    }
                    else
                    {
                        SmartPtr<io::Stream> streamArchive = StreamArchive::Create(stream, true);
                        if (streamArchive.IsValid())
                            stream = streamArchive;

                        bool isAdded = false;
                        bool isArchiveRaw = false;
                        for (;;)
                        {
                            if (addFilesContext->isCancel)
                                break;

                            Array<CommandBuffer::Command> commands;
                            if (auto* replay = replays.Load(stream, commands, entry.type))
                            {
                                isAdded = true;

                                auto* songSheet = new SongSheet;
                                songSheet->type = replay->GetMediaType();
                                auto streamSize = stream->GetSize();
                                songSheet->fileSize = uint32_t(streamSize);
                                if (IS_FILECRC_ENABLED)
                                {
                                    auto fileCrc = crc32(0L, Z_NULL, 0);
                                    auto* moduleData = core::Alloc<uint8_t>(65536);
                                    for (uint64_t s = 0; s < streamSize; s += 65536)
                                    {
                                        auto readSize = stream->Read(moduleData, 65536);
                                        fileCrc = crc32_z(fileCrc, moduleData, size_t(readSize));
                                    }
                                    core::Free(moduleData);
                                    songSheet->fileCrc = fileCrc;
                                }
                                auto numSubsongs = replay->GetNumSubsongs();
                                songSheet->subsongs.Resize(numSubsongs);
                                songSheet->lastSubsongIndex = uint16_t(numSubsongs - 1);
                                for (uint16_t i = 0; i < numSubsongs; i++)
                                {
                                    replay->SetSubsong(i);

                                    auto& subsong = songSheet->subsongs[i];
                                    subsong.Clear();
                                    subsong.durationCs = replay->GetDurationMs() / 10;
                                    subsong.isDirty = false;
                                    if (numSubsongs > 1)
                                    {
                                        subsong.name = replay->GetSubsongTitle();
                                        if (subsong.name.IsEmpty())
                                        {
                                            char txt[32];
                                            sprintf(txt, "subsong %u of %u", i + 1, numSubsongs);
                                            subsong.name = txt;
                                        }
                                    }
                                }
                                songSheet->metadata.Container() = commands;

                                delete replay;

                                std::string artist;
                                if (streamArchive.IsValid())
                                {
                                    if (!isArchiveRaw)
                                        songSheet->sourceIds.Add(SourceID(SourceID::FileImportID, 0));
                                    songSheet->name.String() = stream->GetName();
                                    songSheet->subsongs[0].isArchive = true;
                                    songSheet->subsongs[0].isPackage = true;
                                }
                                else if (!addFilesContext->isUrl)
                                {
                                    TagLib::FileStream fStream(io::File::Convert(entry.path.c_str()).c_str(), true);
                                    TagLib::FileRef f(&fStream);
                                    if (auto* tag = f.tag())
                                    {
                                        if (!tag->title().isEmpty())
                                        {
                                            if (!tag->album().isEmpty())
                                            {
                                                songSheet->name = tag->album().toCString(true);
                                                songSheet->name.String() += '/';

                                                // get id3v2 tags to check if we have a disc number
                                                TagLib::MPEG::File fMpeg(&fStream, true, TagLib::MPEG::Properties::Average, TagLib::ID3v2::FrameFactory::instance());
                                                if (auto* id3v2Tag = fMpeg.ID3v2Tag())
                                                {
                                                    auto properties = id3v2Tag->properties();
                                                    for (auto it = properties.begin(), e = properties.end(); it != e; it++)
                                                    {
                                                        if (_stricmp(it->first.toCString(), "discnumber") == 0)
                                                        {
                                                            uint32_t disc = 0;
                                                            if (sscanf_s(it->second[0].toCString(), "%u", &disc) == 1)
                                                            {
                                                                char buf[16];
                                                                sprintf(buf, "%u ", disc);
                                                                songSheet->name.String() += buf;
                                                            }
                                                            else if (!it->second[0].isEmpty()) // weird format?
                                                            {
                                                                songSheet->name.String() += it->second[0].toCString();
                                                                songSheet->name.String() += ' ';
                                                            }
                                                            break;
                                                        }
                                                    }
                                                }

                                                if (tag->track())
                                                {
                                                    char buf[16];
                                                    sprintf(buf, "%02u ", tag->track());
                                                    songSheet->name.String() += buf;
                                                }
                                                songSheet->name.String() += tag->title().toCString(true);
                                            }
                                            else
                                                songSheet->name = tag->title().toCString(true);
                                        }
                                        songSheet->releaseYear = uint16_t(tag->year());

                                        if (!tag->artist().isEmpty())
                                        {
                                            auto artistTag = tag->artist();
                                            artist = artistTag.toCString(true);
                                        }
                                    }
                                }

                                addFilesContext->Lock();
                                addFilesContext->entriesToUpdate.Add({ songSheet, artist, entry.playlistId });
                                addFilesContext->Unlock();
                            }
                            else if (streamArchive.IsValid())
                            {
                                auto* songSheet = new SongSheet;

                                auto streamSize = stream->GetSize();
                                songSheet->fileSize = uint32_t(streamSize);
                                if (IS_FILECRC_ENABLED)
                                {
                                    auto fileCrc = crc32(0L, Z_NULL, 0);
                                    auto* moduleData = core::Alloc<uint8_t>(65536);
                                    for (uint64_t s = 0; s < streamSize; s += 65536)
                                    {
                                        auto readSize = stream->Read(moduleData, 65536);
                                        fileCrc = crc32_z(fileCrc, moduleData, size_t(readSize));
                                    }
                                    core::Free(moduleData);
                                    songSheet->fileCrc = fileCrc;
                                }
                                songSheet->subsongs[0].isInvalid = true;

                                if (!isArchiveRaw)
                                    songSheet->name.String() = stream->GetName();
                                songSheet->subsongs[0].isArchive = true;
                                songSheet->subsongs[0].isPackage = true;

                                addFilesContext->Lock();
                                addFilesContext->entriesToUpdate.Add({ songSheet, {}, entry.playlistId });
                                addFilesContext->Unlock();
                            }

                            auto newStream = stream->Next(true);
                            if (newStream.IsInvalid())
                            {
                                if (streamArchive.IsInvalid())
                                    streamArchive = newStream = StreamArchiveRaw::Create(stream);
                                if (newStream.IsInvalid())
                                {
                                    if (!isAdded)
                                    {
                                        addFilesContext->Lock();
                                        addFilesContext->entriesToUpdate.Add({ nullptr, {}, entry.playlistId });
                                        addFilesContext->Unlock();
                                    }
                                    break;
                                }
                                isArchiveRaw = true;
                            }
                            stream = std::move(newStream);
                        }
                    }
                }
            }

            addFilesContext->Lock();
            addFilesContext->isDone = true;
            addFilesContext->Unlock();
        });
    }

    void Playlist::UpdateFiles()
    {
        AddFilesContext* prev = nullptr;
        for (auto* addFilesContext = m_addFilesContext; addFilesContext;)
        {
            addFilesContext->Lock();
            auto entries = std::move(addFilesContext->entries);
            auto entriesToUpdate = std::move(addFilesContext->entriesToUpdate);
            auto failedEntries = std::move(addFilesContext->failedEntries);
            auto isDone = addFilesContext->isDone;
            addFilesContext->Unlock();

            for (auto& failedEntry : failedEntries)
                Log::Warning("Playlist: can't find replay for \"%s\"\n", failedEntry.c_str());

            // instead of checking every place where we change the entries (and update the droppedEntryIndex), just clamp or dropped index
            // maybe one day, I'll refactor that and make it clean
            if (addFilesContext->droppedEntryIndex > m_cue.entries.NumItems<int32_t>())
                addFilesContext->droppedEntryIndex = m_cue.entries.NumItems<int32_t>();

            auto currentPlayingEntry = addFilesContext->droppedEntryIndex <= m_currentEntryIndex ? m_cue.entries[m_currentEntryIndex] : MusicID();
            for (uint32_t entryIndex = 0; entryIndex < entries.NumItems(); entryIndex++)
            {
                auto& filename = entries[entryIndex].path;
                if (auto* song = m_cue.db.FindSong([&](auto* song)
                {
                    return _stricmp(m_cue.db.GetPath(song->GetSourceId(0)), filename.c_str()) == 0;
                }))
                {
                    for (uint16_t i = 0, e = song->GetLastSubsongIndex(); i <= e; i++)
                    {
                        if (!song->IsSubsongDiscarded(i))
                        {
                            MusicID musicId;
                            musicId.subsongId.songId = song->GetId();
                            musicId.subsongId.index = i;
                            musicId.playlistId = ++m_uniqueIdGenerator;
                            musicId.databaseId = DatabaseID::kPlaylist;
                            m_cue.entries.Insert(addFilesContext->droppedEntryIndex++, { musicId });
                        }
                    }
                    entries[entryIndex].song->Release();
                    addFilesContext->numEntries--;
                }
                else
                {
                    auto* songSheet = entries[entryIndex].song;
                    songSheet->sourceIds.Add(m_cue.db.AddPath(addFilesContext->isUrl ? SourceID::URLImportID : SourceID::FileImportID, filename));

                    m_cue.db.AddSong(songSheet);

                    MusicID musicId;
                    musicId.subsongId.songId = songSheet->id;
                    musicId.playlistId = ++m_uniqueIdGenerator;
                    musicId.databaseId = DatabaseID::kPlaylist;
                    m_cue.entries.Insert(addFilesContext->droppedEntryIndex++, { musicId });

                    m_cue.db.Raise(Database::Flag::kSaveSongs);

                    addFilesContext->Lock();
                    addFilesContext->entriesToScan.Add({ filename, musicId.playlistId, songSheet->type });
                    addFilesContext->Unlock();
                }
            }
            for (auto& entryToUpdate : entriesToUpdate)
            {
                if (auto* entry = m_cue.entries.FindIf([&entryToUpdate](auto& entry)
                {
                    return entry == entryToUpdate.playlistId;
                }))
                {
                    auto* songSheet = entry->GetSong()->Edit();
                    if (entryToUpdate.song == nullptr)
                    {
                        songSheet->subsongs[0].isInvalid = 1;
                        if (entryToUpdate.artist.empty())
                            Log::Warning("Playlist: can't find replay for \"%s\"\n", m_cue.db.GetPath(songSheet->sourceIds[0]));
                        else
                            Log::Warning("Playlist: can't opened \"%s\"\n", m_cue.db.GetPath(songSheet->sourceIds[0]));
                    }
                    else
                    {
                        if (entryToUpdate.song->subsongs[0].isArchive)
                        {
                            auto isArchiveRaw = entryToUpdate.song->sourceIds.IsEmpty();
                            if (entryToUpdate.song->subsongs[0].isInvalid)
                            {
                                if (entryToUpdate.song->name.IsEmpty())
                                    Log::Warning("Playlist: can't find replay for \"%s\"\n", m_cue.db.GetPath(songSheet->sourceIds[0]));
                                else
                                    Log::Warning("Playlist: can't find replay for \"%s:%s\"\n", m_cue.db.GetPath(songSheet->sourceIds[0]), entryToUpdate.song->name.String().c_str());
                            }
                            if (addFilesContext->previousSongId == songSheet->id)
                            {
                                m_cue.db.AddSong(entryToUpdate.song);

                                MusicID musicId;
                                musicId.subsongId.songId = entryToUpdate.song->id;
                                musicId.playlistId = ++m_uniqueIdGenerator;
                                musicId.databaseId = DatabaseID::kPlaylist;

                                entry = m_cue.entries.FindIf([addFilesContext](auto& entry)
                                {
                                    return entry == addFilesContext->previousPlaylistId;
                                });
                                if (entry)
                                    entry = m_cue.entries.Insert(entry - m_cue.entries + 1, { musicId });
                                else
                                    entry = m_cue.entries.Add({ musicId });

                                entryToUpdate.song->sourceIds.Clear();
                                entryToUpdate.song->sourceIds.Add(songSheet->sourceIds[0]);
                                entryToUpdate.song->databaseDay = songSheet->databaseDay;
                                songSheet = entryToUpdate.song;
                            }
                            else
                                addFilesContext->previousSongId = songSheet->id;

                            if (!isArchiveRaw) // ArchiveRaw doesn't have extra file (for now)
                                songSheet->sourceIds.Add(m_cue.db.AddPath(SourceID::FileImportID, entryToUpdate.song->name.String()));

                            songSheet->subsongs[0].isDirty = true;
                            std::string name = reinterpret_cast<const char*>(std::filesystem::path(m_cue.db.GetPath(songSheet->sourceIds[0])).filename().u8string().c_str());
                            if (entryToUpdate.song->name.IsNotEmpty())
                            {
                                name += ":";
                                entryToUpdate.song->name = name + entryToUpdate.song->name.String();
                            }
                            else
                                entryToUpdate.song->name = name;
                        }

                        if (!entryToUpdate.artist.empty() && songSheet->artistIds.IsEmpty())
                        {
                            Artist* dbArtist = nullptr;
                            auto* artistName = entryToUpdate.artist.c_str();
                            for (auto* artist : m_cue.db.Artists())
                            {
                                if (strcmp(artistName, artist->GetHandle()) == 0)
                                {
                                    dbArtist = artist;
                                    break;
                                }
                            }
                            if (dbArtist == nullptr)
                            {
                                auto* artistSheet = new ArtistSheet;
                                artistSheet->handles.Add(artistName);
                                dbArtist = m_cue.db.AddArtist(artistSheet);
                            }
                            songSheet->artistIds.Add(dbArtist->GetId());
                            dbArtist->Edit()->numSongs++;
                        }

                        auto playlistId = entry->playlistId;
                        if (songSheet->subsongs[0].isDirty)
                        {
                            auto playlistIndex = entry - m_cue.entries;
                            songSheet->fileSize = entryToUpdate.song->fileSize;
                            songSheet->fileCrc = entryToUpdate.song->fileCrc;
                            songSheet->lastSubsongIndex = entryToUpdate.song->lastSubsongIndex;
                            songSheet->type = entryToUpdate.song->type;
                            if (entryToUpdate.song->name.IsNotEmpty())
                                songSheet->name = entryToUpdate.song->name;
                            songSheet->metadata = entryToUpdate.song->metadata;
                            songSheet->releaseYear = entryToUpdate.song->releaseYear;
                            songSheet->subsongs = entryToUpdate.song->subsongs;
                            for (uint16_t i = 1, e = songSheet->lastSubsongIndex; i <= e; i++)
                            {
                                MusicID musicId;
                                musicId.subsongId.songId = songSheet->id;
                                musicId.subsongId.index = i;
                                musicId.playlistId = ++m_uniqueIdGenerator;
                                musicId.databaseId = DatabaseID::kPlaylist;
                                m_cue.entries.Insert(++playlistIndex, { musicId });
                            }
                            addFilesContext->previousPlaylistId = m_cue.entries[playlistIndex].playlistId;
                        }
                        else
                            addFilesContext->previousPlaylistId = entryToUpdate.playlistId;

                        if (entryToUpdate.playlistId == playlistId)
                            delete entryToUpdate.song;
                    }

                    m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                }
                else if (entryToUpdate.song)
                    delete entryToUpdate.song;
            }
            if (currentPlayingEntry.subsongId.IsValid())
                m_currentEntryIndex = m_cue.entries.Find<uint32_t>(currentPlayingEntry.playlistId);
            auto* next = addFilesContext->next;
            if (isDone)
            {
                delete addFilesContext;
                if (!prev)
                    m_addFilesContext = next;
                else
                    prev->next = next;
            }
            else
                prev = addFilesContext;
            addFilesContext = next;
        }
    }

    std::string Playlist::GetPlaylistFilename(const std::string& name)
    {
        std::string filename = MusicPath;
        filename += "playlists.";
        filename += name;
        filename += MusicExt;
        return filename;
    }

    void Playlist::Flush()
    {
        for (auto* addFilesContext = m_addFilesContext; addFilesContext; addFilesContext = addFilesContext->next)
            addFilesContext->isCancel = true;
        while (m_addFilesContext)
            UpdateFiles();
    }

    void Playlist::Save()
    {
        if (m_cue.db.Fetch().value)
            SavePlaylistsToc();
        else if (m_oldCurrentEntryIndex != m_currentEntryIndex)
            PatchPlaylistsToc();
    }

    void Playlist::SavePlaylistsToc()
    {
        auto file = io::File::OpenForWrite(ms_fileName);
        if (file.IsValid())
        {
            file.Write(kMusicFileStamp);
            file.Write(Core::GetVersion());

            file.WriteAs<uint32_t>(m_cue.entries.NumItems());
            file.Write(m_currentEntryIndex);
            m_oldCurrentEntryIndex = m_currentEntryIndex;

            file.WriteAs<uint32_t>(m_playlists.NumItems());
            for (auto& summary : m_playlists)
                summary.Save(file);

            SavePlaylist(file, m_cue);
        }
    }

    void Playlist::PatchPlaylistsToc()
    {
        if (io::File::IsExisting(ms_fileName))
        {
            auto file = io::File::OpenForAppend(ms_fileName);
            if (file.IsValid())
            {
                file.Seek(12);
                file.Write(m_currentEntryIndex);
                m_oldCurrentEntryIndex = m_currentEntryIndex;
            }
        }
        else
            SavePlaylistsToc();
    }
}
// namespace rePlayer