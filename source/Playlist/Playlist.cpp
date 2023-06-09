#include "Playlist.h"

// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ImGui/imgui_internal.h>
#include <IO/File.h>
#include <IO/StreamFile.h>

// rePlayer
#include <Database/Database.h>
#include <Database/DatabaseArtistsUI.h>
#include <Database/SongEditor.h>
#include <Deck/Deck.h>
#include <Deck/Player.h>
#include <Library/Library.h>
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

    const char* const Playlist::ms_fileName = MusicPath "playlists" MusicExt;

    Playlist::Playlist()
        : Window("Playlist", ImGuiWindowFlags_NoCollapse)
        , m_cue{ Core::GetDatabase(DatabaseID::kPlaylist) }
        , m_dropTarget(new DropTarget(*this))
        , m_artists(new DatabaseArtistsUI(DatabaseID::kPlaylist, *this))
        , m_songs(new SongsUI(m_cue.paths, *this))
    {
        auto file = io::File::OpenForRead(ms_fileName);
        if (file.IsValid())
        {
            if (file.Read<uint64_t>() == Summary::kVersion)
            {
                m_cue.entries.Resize(file.Read<uint32_t>());
                m_oldCurrentEntryIndex = m_currentEntryIndex = file.Read<int32_t>();

                m_playlists.Resize(file.Read<uint32_t>());
                for (auto& summary : m_playlists)
                    summary.Load(file);

                auto status = LoadPlaylist(file, m_cue);
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
        m_cue.paths = {};
        m_cue.entries.Clear();
        m_oldCurrentEntryIndex = m_currentEntryIndex = -1;
        m_uniqueIdGenerator = PlaylistID::kInvalid;
        m_lastSelectedEntry = PlaylistID::kInvalid;

        Core::GetDeck().OnNewPlaylist();
    }

    void Playlist::Enqueue(MusicID musicId)
    {
        musicId.playlistId = ++m_uniqueIdGenerator;
        m_cue.entries.Add(musicId);
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
                    Database db;
                    Cue cue(db);

                    cue.entries.Resize(summary.numSubsongs);
                    if (file.Read<uint64_t>() != kVersion || LoadPlaylist(file, cue) != Status::kOk)
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
                    if (numEntries == 0)
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

                            file.Write(kVersion);
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
        return io::StreamFile::Create(m_cue.paths.Items(song->GetSourceId(0).internalId));
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
            auto stream = io::StreamFile::Create(m_cue.paths.Items(song->sourceIds[0].internalId));
            if (stream.IsValid())
            {
                if (dbSong->GetFileSize() == 0)
                {
                    auto moduleData = stream->Read();
                    song->fileSize = uint32_t(moduleData.Size());
                    auto fileCrc = crc32(0L, Z_NULL, 0);
                    song->fileCrc = crc32_z(fileCrc, moduleData.Items(), moduleData.Size());
                    song->subsongs[0].isDirty = true;
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
                        Log::Message("%s: loaded %06X%02X \"%s\"\n", Core::GetReplays().GetName(song->type.replay), uint32_t(musicId.subsongId.songId), uint32_t(musicId.subsongId.index), m_cue.paths.Items(song->sourceIds[0].internalId));
                    }
                    else
                    {
                        delete replay;
                        Log::Message("%s: discarded %06X%02X \"%s\"\n", Core::GetReplays().GetName(song->type.replay), uint32_t(musicId.subsongId.songId), uint32_t(musicId.subsongId.index), m_cue.paths.Items(song->sourceIds[0].internalId));
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
                    Log::Error("Can't find a replay for \"%s\"\n", m_cue.paths.Items(song->sourceIds[0].internalId));
                }
            }
            else
            {
                if (!song->subsongs[0].isUnavailable)
                {
                    song->subsongs[0].isUnavailable = true;
                    m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                }
                Log::Error("Can't open file \"%s\"\n", m_cue.paths.Items(song->sourceIds[0].internalId));
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
                        return LoadNextSong(isAdvancing);
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
        sprintf(title, "Playlist: %d/%d %s - %d:%02d:%02d###Playlist", m_currentEntryIndex + 1, m_cue.entries.NumItems(), m_cue.entries.NumItems() > 1 ? "entries" : "entry", duration / 3600, (duration % 3600) / 60, duration % 60);

        ImGui::SetNextWindowPos(ImVec2(16.0f, 188.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(430.0f, 480.0f), ImGuiCond_FirstUseEver);

        return title;
    }

    void Playlist::OnDisplay()
    {
        auto& deck = Core::GetDeck();

        m_dropTarget->UpdateDragDropSource(0);

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
        if (ImGui::BeginTable("tabs", numColumns, resizeFlags | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings))
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
                        if (ImGui::Selectable("##select", curEntry.isSelected, selectable_flags, ImVec2(0.0f, ImGui::TableGetInstanceData(ImGui::GetCurrentTable(), ImGui::GetCurrentTable()->InstanceCurrent)->LastFirstRowHeight)))//TBD: using imgui_internal for row height
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
                                ProcessExternalDranAndDrop(insertPos);
                            }

                            if (auto payload = ImGui::AcceptDragDropPayload("DATABASE"))
                            {
                                auto mousePos = ImGui::GetMousePos();
                                auto insertPos = abs(mousePos.y - selectableMin.y) < abs(mousePos.y - selectableMax.y) ? rowIdx : rowIdx + 1;
                                auto currentPlayingEntry = insertPos <= m_currentEntryIndex ? m_cue.entries[m_currentEntryIndex] : Cue::Entry();
                                auto selectedSongs = reinterpret_cast<MusicID*>(payload->Data);
                                for (uint32_t i = 0, e = payload->DataSize / sizeof(MusicID); i < e; i++)
                                    m_cue.entries.Insert(insertPos + i, selectedSongs[i])->playlistId = ++m_uniqueIdGenerator;
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
                        ImGui::Text("%s", song->GetType().GetExtension());
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
                    if (auto payload = ImGui::AcceptDragDropPayload("DATABASE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                    {
                        auto selectedSongs = reinterpret_cast<MusicID*>(payload->Data);
                        for (uint32_t i = 0, e = payload->DataSize / sizeof(MusicID); i < e; i++)
                            m_cue.entries.Add(selectedSongs[i])->playlistId = ++m_uniqueIdGenerator;
                        m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Delete)))
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
            ProcessExternalDranAndDrop(m_cue.entries.NumItems<int32_t>());

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

            auto holdEntries = reinterpret_cast<Cue::Entry*>(_alloca(dt * sizeof(Cue::Entry)));
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

            auto holdEntries = reinterpret_cast<Cue::Entry*>(_alloca(dt * sizeof(Cue::Entry)));
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

    void Playlist::ProcessExternalDranAndDrop(int32_t droppedEntryIndex)
    {
        if (m_dropTarget->IsOverriding())
            Clear();

        auto& replays = Core::GetReplays();
        auto currentPlayingEntry = droppedEntryIndex <= m_currentEntryIndex ? m_cue.entries[m_currentEntryIndex] : MusicID();
        bool isAcceptingAll = m_dropTarget->IsAcceptingAll();

        auto databaseDay = uint16_t((std::chrono::sys_days(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())) - std::chrono::sys_days(std::chrono::days(Core::kReferenceDate))).count());
        auto files = m_dropTarget->AcquireFiles();
        for (auto filename : files)
        {
            auto addFile = [&](const std::filesystem::path& path)
            {
                // add only known extensions (or prefixes)
                auto extension = path.extension().string();
                if (!isAcceptingAll)
                {
                    auto stem = path.stem().string();
                    for (uint16_t i = 0;; i++)
                    {
                        if (i == uint16_t(eExtension::Count))
                        {
                            // look for it the hard way
                            auto stream = io::StreamFile::Create(reinterpret_cast<const char*>(path.u8string().c_str()));
                            Array<CommandBuffer::Command> commands;
                            if (auto* replay = replays.Load(stream, commands, {}))
                            {
                                extension = ".";
                                extension += replay->GetMediaType().GetExtension();
                                delete replay;
                                break;
                            }
                            return;
                        }
                        if (extension.size() > 1 && _stricmp(extension.c_str() + 1, MediaType::extensionNames[i]) == 0)
                            break;
                        if (_strnicmp(MediaType::extensionNames[i], stem.c_str(), MediaType::extensionLengths[i]) == 0)
                        {
                            if (MediaType::extensionLengths[i] == stem.size() || stem[MediaType::extensionLengths[i]] == '.')
                            {
                                extension = ".";
                                extension += MediaType::extensionNames[i];
                                break;
                            }
                        }
                    }
                }

                // add to playlist
                std::string filename = reinterpret_cast<const char*>(path.u8string().c_str());
                if (auto song = m_cue.db.FindSong([&](auto* song)
                {
                    return _stricmp(m_cue.paths.Items(song->GetSourceId(0).internalId), filename.c_str()) == 0;
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
                            m_cue.entries.Insert(droppedEntryIndex++, musicId);
                        }
                    }
                }
                else
                {
                    auto* songSheet = new SongSheet;
                    songSheet->type = replays.Find(extension.c_str() + 1);
                    if (songSheet->type.ext == eExtension::Unknown)
                        songSheet->name = path.filename().string().c_str();
                    else if (_stricmp(path.extension().string().c_str() + 1, songSheet->type.GetExtension()) == 0)
                        songSheet->name = path.stem().string().c_str();
                    else
                    {
                        auto name = path.filename().string();
                        auto extSize = songSheet->type.extensionLengths[size_t(songSheet->type.ext)];
                        if (_strnicmp(name.c_str(), songSheet->type.GetExtension(), extSize) == 0 && name.size() > extSize && name.c_str()[extSize] == '.')
                            songSheet->name = name.c_str() + extSize + 1;
                        else
                            songSheet->name = name.c_str();
                    }
                    songSheet->sourceIds.Add(SourceID(SourceID::FileImportID, m_cue.paths.NumItems()));
                    songSheet->databaseDay = databaseDay;
                    m_cue.paths.Add(filename.c_str(), filename.size() + 1);
                    m_cue.arePathsDirty = true;

                    m_cue.db.AddSong(songSheet);

                    // TODO: make this async
                    TagLib::FileStream fStream(path.c_str(), true);
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
                                TagLib::MPEG::File fMpeg(&fStream, TagLib::ID3v2::FrameFactory::instance());
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
                            Artist* dbArtist = nullptr;
                            auto artistTag = tag->artist();
                            auto artistName = artistTag.toCString(true);
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
                    }

                    MusicID musicId;
                    musicId.subsongId.songId = songSheet->id;
                    musicId.playlistId = ++m_uniqueIdGenerator;
                    musicId.databaseId = DatabaseID::kPlaylist;
                    m_cue.entries.Insert(droppedEntryIndex++, musicId);
                }

                m_cue.db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);
            };

            auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(filename.c_str()));
            if (std::filesystem::is_directory(path))
            {
                for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(path))
                {
                    if (!dir_entry.is_directory())
                        addFile(dir_entry.path());
                }
            }
            else
                addFile(path);
        }

        if (currentPlayingEntry.subsongId.IsValid())
            m_currentEntryIndex = m_cue.entries.Find<uint32_t>(currentPlayingEntry.playlistId);
    }

    Status Playlist::LoadPlaylist(io::File& file, Cue& cue)
    {
        for (uint32_t i = 0, e = cue.entries.NumItems(); i < e; i++)
        {
            SubsongID subsongId;
            file.Read(subsongId.value);
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

        auto status = Status::kOk;
        status = cue.db.LoadSongs(file);
        if (status == Status::kOk)
        {
            status = cue.db.LoadArtists(file);
            if (status == Status::kOk)
                file.Read<uint32_t>(cue.paths);
        }

        return status;
    }

    void Playlist::SavePlaylist(io::File& file, const Cue& cue)
    {
        for (auto& entry : cue.entries)
            file.Write(entry.subsongId.value);

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

        cue.db.SaveSongs(file);
        cue.db.SaveArtists(file);
        file.Write<uint32_t>(cue.paths);
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

                            if (file.Read<uint64_t>() == kVersion)
                                LoadPlaylist(file, m_cue);
                            else
                                assert(0 && "file read error");

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
                    file.Write(kVersion);
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
                        for (uint32_t entryIdx = 0, count = m_cue.entries.NumItems(); entryIdx < count; entryIdx++)
                            std::swap(m_cue.entries[entryIdx], m_cue.entries[entryIdx + std::rand() % (count - entryIdx)]);
                    }
                    else if (i == 6)
                    {
                        for (int32_t n = 0, j = m_cue.entries.NumItems<int32_t>() - 1; n < j; n++, j--)
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
                        std::sort(m_cue.entries.begin(), m_cue.entries.end(), [this, i](const MusicID& l, const MusicID& r)
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
                        return l.GetId() < r.GetId();
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

    std::string Playlist::GetPlaylistFilename(const std::string& name)
    {
        std::string filename = MusicPath;
        filename += "playlists.";
        filename += name;
        filename += MusicExt;
        return filename;
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
            file.Write(Summary::kVersion);

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