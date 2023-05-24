#include "Library.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ImGui/imgui_internal.h>//ImGui::GetCurrentTable for first row height
#include <ImGui/ImGuiFileDialog.h>
#include <IO/File.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>

#include <Database/Database.h>
#include <Database/SongEditor.h>
#include <Database/Types/Countries.h>
#include <Deck/Deck.h>
#include <Deck/Player.h>
#include <Library/LibraryArtistsUI.h>
#include <Library/LibrarySongsUI.h>
#include <Library/Sources/AmigaMusicPreservation.h>
#include <Library/Sources/AtariSAPMusicArchive.h>
#include <Library/Sources/FileImport.h>
#include <Library/Sources/HighVoltageSIDCollection.h>
#include <Library/Sources/Modland.h>
#include <Library/Sources/SNDH.h>
#include <Library/Sources/TheModArchive.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

#include <zlib.h>

#include <algorithm>
#include <chrono>

namespace rePlayer
{
    const char* const Library::ms_songsFilename = MusicPath "songs" MusicExt;
    const char* const Library::ms_artistsFilename = MusicPath "artists" MusicExt;

    Library::Library()
        : Window("Library", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)
        , m_db(Core::GetDatabase(DatabaseID::kLibrary))
        , m_artists(new ArtistsUI(*this))
        , m_songs(new SongsUI(*this))
    {
        m_sources[SourceID::AmigaMusicPreservationSourceID] = new SourceAmigaMusicPreservation();
        m_sources[SourceID::TheModArchiveSourceID] = new SourceTheModArchive();
        m_sources[SourceID::ModlandSourceID] = new SourceModland();
        m_sources[SourceID::FileImportID] = new SourceFileImport();
        m_sources[SourceID::HighVoltageSIDCollectionID] = new SourceHighVoltageSIDCollection();
        m_sources[SourceID::SNDHID] = new SourceSNDH();
        m_sources[SourceID::AtariSAPMusicArchiveID] = new SourceAtariSAPMusicArchive();

        Load();

        Enable(true);
    }

    Library::~Library()
    {
        m_songs->CleanupCache();

        for (auto source : m_sources)
            delete source;

        delete m_artists;
        delete m_songs;
    }

    SmartPtr<core::io::Stream> Library::GetStream(Song* song)
    {
        auto filename = m_songs->GetFullpath(song);

        SmartPtr<io::Stream> stream = song->GetFileSize() > 0 ? io::StreamFile::Create(filename) : nullptr;
        if (stream.IsInvalid())
        {
            auto songSheet = song->Edit();

            // we need to import because the file is not in the cache
            if (songSheet->sourceIds[0].sourceId == SourceID::FileImportID)
            {
                // the file in the cache was a file import, if there are other sources, discard the main one, otherwise set this song as invalid
                if (songSheet->sourceIds.NumItems() == 1)
                {
                    if (!songSheet->subsongs[0].isInvalid)
                    {
                        songSheet->subsongs[0].isInvalid = true;
                        m_db.Raise(Database::Flag::kSaveSongs);
                    }
                    Log::Warning("Imported file is missing for ID_%06X \"[%s]%s\"\n", uint32_t(songSheet->id), songSheet->type.GetExtension(), m_db.GetTitleAndArtists(songSheet->id).c_str());
                    return {};
                }
                Log::Warning("Discarding imported file for ID_%06X \"[%s]%s\"\n", uint32_t(songSheet->id), songSheet->type.GetExtension(), m_db.GetTitleAndArtists(songSheet->id).c_str());
                songSheet->sourceIds.RemoveAt(0);
                m_db.Raise(Database::Flag::kSaveSongs);
            }

            // download the song
            for (uint32_t i = 0; i < songSheet->sourceIds.NumItems(); i++)
            {
                auto sourceId = songSheet->sourceIds[i];
                auto module = m_sources[sourceId.sourceId]->ImportSong(sourceId);
                auto& moduleData = module.first;
                if (moduleData.IsNotEmpty())
                {
                    // if the first source wasn't available, the next available becomes the default
                    if (i != 0)
                    {
                        std::swap(songSheet->sourceIds[0], songSheet->sourceIds[i]);
                        m_sources[songSheet->sourceIds[0].sourceId]->DiscardSong(songSheet->sourceIds[0], songSheet->id);
                        m_sources[sourceId.sourceId]->InvalidateSong(sourceId, songSheet->id);
                    }

                    // build the crc of the file
                    auto fileSize = static_cast<uint32_t>(moduleData.Size());
                    auto fileCrc = crc32(0L, Z_NULL, 0);
                    fileCrc = crc32_z(fileCrc, moduleData.Items(), moduleData.Size());
                    if (fileSize != songSheet->fileSize || fileCrc != songSheet->fileCrc)
                    {
                        // file has changed
                        songSheet->subsongs[0].isDirty = true;
                        songSheet->fileSize = fileSize;
                        songSheet->fileCrc = fileCrc;
                    }

                    // save the downloaded song
                    auto file = io::File::OpenForWrite(filename.c_str());
                    file.Write(moduleData.Items(), moduleData.Size());

                    // create the stream from the data
                    stream = io::StreamMemory::Create(filename, moduleData.Items(), moduleData.Size(), false);
                    moduleData.Detach();

                    break;
                }
                else if (module.second)
                {
                    --i;
                    songSheet->sourceIds.RemoveAt(0);
                    if (songSheet->sourceIds.IsEmpty())
                    {
                        for (uint16_t subsongIdx = 0; subsongIdx <= songSheet->lastSubsongIndex; subsongIdx++)
                        {
                            if (!songSheet->subsongs[subsongIdx].isDiscarded)
                                m_songs->DeleteSubsong(SubsongID(songSheet->id, subsongIdx));
                        }
                    }
                }
            }
        }
        return stream;
    }

    SmartPtr<Player> Library::LoadSong(const MusicID musicId)
    {
        assert(musicId.databaseId == DatabaseID::kLibrary);
        SmartPtr<Player> player;
        auto dbSong = m_db[musicId.subsongId];
        if (dbSong == nullptr)
            return player;

        auto* song = dbSong->Edit();
        SmartPtr<io::Stream> stream = GetStream(dbSong);
        if (stream.IsInvalid())
        {
            // can't load it, tag it
            if (!song->subsongs[0].isUnavailable)
            {
                song->subsongs[0].isUnavailable = true;
                m_db.Raise(Database::Flag::kSaveSongs);
            }
            return player;
        }

        // song is available, try to play it
        auto metadata(song->metadata);
        if (auto replay = Core::GetReplays().Load(stream, song->metadata.Container(), song->type))
        {
            auto oldType = song->type;
            auto type = replay->GetMediaType();
            bool hasChanged = oldType != type || metadata != song->metadata;
            song->type = type;
            if (oldType.ext != type.ext)
            {
                auto newFilename = m_songs->GetFullpath(dbSong);
                if (!io::File::Rename(stream->GetName().c_str(), newFilename.c_str()))
                {
                    Log::Warning("Can't rename file \"%s\"\n", stream->GetName().c_str());
                    m_songs->InvalidateCache();
                    io::File::Copy(stream->GetName().c_str(), newFilename.c_str());
                }
            }
            uint32_t oldNumSubsongs = song->lastSubsongIndex + 1ul;
            auto numSubsongs = replay->GetNumSubsongs();
            if (numSubsongs != oldNumSubsongs || song->subsongs[0].isDirty || song->subsongs[0].isInvalid || oldType.replay != type.replay)
            {
                hasChanged = true;

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
                    if (musicId.subsongId.index != i)
                        Core::Discard(MusicID(SubsongID(song->id, i), DatabaseID::kLibrary));
                }
            }
            else if (song->subsongs[0].isUnavailable)
            {
                hasChanged = true;
                song->subsongs[0].isUnavailable = false;
            }
            if (musicId.subsongId.index < numSubsongs)
            {
                player = Player::Create(musicId, song, replay, stream);
                player->MarkSongAsNew(hasChanged);
                Log::Message("%s: loaded %06X%02X \"%s.%s\"\n", Core::GetReplays().GetName(song->type.replay), uint32_t(musicId.subsongId.songId), uint32_t(musicId.subsongId.index), m_db.GetTitleAndArtists(musicId.subsongId).c_str(), song->type.GetExtension());
            }
            else
            {
                delete replay;
                Log::Message("%s: discarded %06X%02X \"%s.%s\"\n", Core::GetReplays().GetName(song->type.replay), uint32_t(musicId.subsongId.songId), uint32_t(musicId.subsongId.index), m_db.GetTitleAndArtists(musicId.subsongId).c_str(), song->type.GetExtension());
            }
            if (hasChanged)
                m_db.Raise(Database::Flag::kSaveSongs);
        }
        else
        {
            // unplayable, tag it
            if (!song->subsongs[0].isInvalid)
            {
                song->subsongs[0].isInvalid = true;
                m_db.Raise(Database::Flag::kSaveSongs);
            }
            Log::Warning("Can't find a suitable replay for ID_%06X \"[%s]%s\"\n", uint32_t(song->id), song->type.GetExtension(), m_db.GetTitleAndArtists(musicId.subsongId).c_str());
        }
        return player;
    }

    std::string Library::OnGetWindowTitle()
    {
        char title[128];
        sprintf(title, "Library: %u/%u songs (%u packages) - %u artists###Library", m_songs->NumSelectedSubsongs(), m_songs->NumSubsongs(), m_db.NumSongs(), m_db.NumArtists());

        ImGui::SetNextWindowPos(ImVec2(480.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(624.0f, 660.0f), ImGuiCond_FirstUseEver);

        return title;
    }

    void Library::OnDisplay()
    {
        if (ImGui::BeginTabBar("LibraryBar", ImGuiTabBarFlags_None))
        {
            ImGui::PushStyleColor(ImGuiCol_Tab, (ImVec4)ImColor::HSV(5.0f / 7.0f, 0.6f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_TabHovered, (ImVec4)ImColor::HSV(5.0f / 7.0f, 0.7f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_TabActive, (ImVec4)ImColor::HSV(5.0f / 7.0f, 0.8f, 1.0f));
            if (ImGui::TabItemButton("Import", ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_NoTooltip))
                ImGui::OpenPopup("Import");
            ImGui::PopStyleColor(3);
            UpdateImports();

            if (ImGui::BeginTabItem("Songs"))
            {
                m_songs->OnDisplay();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Artists"))
            {
                m_artists->OnDisplay();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void Library::OnEndUpdate()
    {
        m_songs->OnEndUpdate();
    }

    void Library::SourceSelection()
    {
        if (ImGui::Button("Sources"))
            ImGui::OpenPopup("Sources");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            for (uint16_t i = 0; i < SourceID::NumSourceIDs; i++)
            {
                if (m_selectedSources & (1ull << i))
                    ImGui::TextUnformatted(SourceID::sourceNames[i]);
            }
            ImGui::EndTooltip();
        }
        if (ImGui::BeginPopup("Sources"))
        {
            auto selectedSources = m_selectedSources;
            for (uint16_t i = 0; i < SourceID::NumSourceIDs; i++)
            {
                if (i == SourceID::FileImportID)
                    continue;
                bool isChecked = selectedSources & (1ull << i);
                if (ImGui::Checkbox(SourceID::sourceNames[i], &isChecked))
                {
                    if (ImGui::GetIO().KeyCtrl)
                    {
                        if (isChecked)
                            selectedSources = 1ull << i;
                        else
                            selectedSources = ~(1ull << i) & kSelectableSources;
                    }
                    else
                    {
                        selectedSources ^= 1ull << i;
                        if (selectedSources == 0)
                            selectedSources = ~(1ull << i) & kSelectableSources;
                    }
                }
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Press Ctrl for exclusive check");
            }
            m_selectedSources = selectedSources;

            ImGui::EndPopup();
        }
    }

    void Library::UpdateImports()
    {
        if (ImGui::BeginPopup("Import"))
        {
            if (ImGui::Selectable("Artists"))
            {
                m_imports = {};
                m_importArtists.isOpened = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Songs"))
            {
                m_imports = {};
                m_importSongs.isOpened = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Files"))
            {
                std::string filters = "All Files (*.*){.*},";
                filters += Core::GetReplays().GetFileFilters();
                ImGuiFileDialog::Instance()->OpenDialog("ImportFiles", "Import Files", filters.c_str(), m_lastFileDialogPath, 0, nullptr, ImGuiFileDialogFlags_DisableCreateDirectoryButton);

                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (m_importArtists.isOpened)
            ImGui::OpenPopup("Import Artists");
        else if (m_importSongs.isOpened)
            ImGui::OpenPopup("Import Songs");

        UpdateImportArtists();

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(480.f, 240.f), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Import Songs", &m_importSongs.isOpened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar))
        {
            if (ImGui::BeginTable("Toolbar", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Search");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText("##Search", &m_importSongs.searchName, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    m_imports = {};
                    for (uint32_t i = 0; i < SourceID::NumSourceIDs; i++)
                    {
                        if (m_selectedSources & (1ull << i))
                            m_sources[i]->FindSongs(m_importSongs.searchName.c_str(), m_imports.sourceResults);
                    }
                    m_imports.isOpened = &m_importSongs.isOpened;
                }
                ImGui::TableNextColumn();
                SourceSelection();
                ImGui::EndTable();
            }

            ProcessImports();

            ImGui::EndPopup();
        }

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        if (ImGuiFileDialog::Instance()->Display("ImportFiles"))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                for (auto& path : ImGuiFileDialog::Instance()->GetSelection())
                {
                    auto stream = io::StreamFile::Create(path.second.c_str());
                    if (stream.IsInvalid())
                        continue;

                    auto fileSize = static_cast<uint32_t>(stream->GetSize());
                    if (fileSize == 0)
                        continue;

                    auto moduleData = stream->Read();

                    // search if it has already been added
                    auto fileCrc = crc32(0L, Z_NULL, 0);
                    fileCrc = crc32_z(fileCrc, moduleData.Items(), moduleData.Size());
                    for (auto* song : m_db.Songs())
                    {
                        if (song->GetFileSize() == fileSize && song->GetFileCrc() == fileCrc)
                        {
                            auto file = io::File::OpenForRead(m_songs->GetFullpath(song).c_str());
                            Array<uint8_t> otherData;
                            otherData.Resize(file.GetSize());
                            file.Read(otherData.Items(), otherData.Size());
                            if (memcmp(moduleData.Items(), otherData.Items(), fileSize) == 0)
                            {
                                Log::Error("File \"%s\" already imported as \"[%s]%s\"\n", path.second.c_str(), song->GetType().GetExtension(), m_db.GetTitleAndArtists(song->GetId()).c_str());

                                stream.Reset();
                                break;
                            }
                        }
                    }

                    if (stream.IsInvalid())
                        continue;

                    Log::Message("Import: \"%s\"\n", path.second.c_str());

                    auto* song = new SongSheet;
                    auto* dbSong = m_db.AddSong(song);

                    // guess the song type
                    std::string songName = path.first.c_str();
                    auto extOffset = songName.find_last_of('.');
                    if (extOffset != songName.npos)
                    {
                        song->type = { songName.c_str() + extOffset + 1, eReplay::Unknown };
                        if (song->type.ext != eExtension::Unknown)
                            songName.resize(extOffset);
                    }
                    song->name = songName;

                    // build the crc of the file
                    song->fileSize = fileSize;
                    song->fileCrc = fileCrc;

                    // null source
                    song->sourceIds.Add(SourceID(SourceID::FileImportID, 0));

                    // save file
                    auto file = io::File::OpenForWrite(m_songs->GetFullpath(dbSong).c_str());
                    file.Write(moduleData.Items(), moduleData.Size());

                    m_db.Raise(Database::Flag::kSaveSongs);
                }
            }
            m_lastFileDialogPath = ImGuiFileDialog::Instance()->GetCurrentPath();
            ImGuiFileDialog::Instance()->Close();
        }
    }

    void Library::UpdateImportArtists()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(480.f, 240.f), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Import Artists", &m_importArtists.isOpened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar))
        {
            if (m_imports.sourceResults.songs.IsEmpty())
                FindArtists();
            else
                ProcessImports();

            ImGui::EndPopup();
        }
    }

    void Library::FindArtists()
    {
        if (ImGui::BeginTable("Toolbar", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Search");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##Search", &m_importArtists.searchName, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                Source::ArtistsCollection collection;
                for (uint32_t i = 0; i < SourceID::NumSourceIDs; i++)
                {
                    if (m_selectedSources & (1ull << i))
                        m_sources[i]->FindArtists(collection, m_importArtists.searchName.c_str());
                }
                std::sort(collection.matches.begin(), collection.matches.end(), [this](auto& l, auto& r)
                {
                    return _stricmp(l.name.c_str(), r.name.c_str()) < 0;
                });
                std::sort(collection.alternatives.begin(), collection.alternatives.end(), [this](auto& l, auto& r)
                {
                    return _stricmp(l.name.c_str(), r.name.c_str()) < 0;
                });
                m_importArtists.artists = std::move(collection.matches);
                m_importArtists.artists.Reserve(collection.alternatives.NumItems());
                for (auto& artist : collection.alternatives)
                    m_importArtists.artists.Add(std::move(artist));
                m_importArtists.states.Resize(m_importArtists.artists.NumItems());

                m_importArtists.lastSelected = -1;

                for (uint32_t i = 0, e = m_importArtists.artists.NumItems(); i < e; i++)
                {
                    auto& importArtist = m_importArtists.artists[i];
                    auto& state = m_importArtists.states[i];
                    state.isSelected = false;
                    state.isImported = false;
                    state.id = ArtistID::Invalid;

                    for (auto* dbArtist : m_db.Artists())
                    {
                        for (auto& source : dbArtist->Sources())
                        {
                            if (importArtist.id == source.id)
                            {
                                state.id = dbArtist->GetId();
                                break;
                            }
                        }
                        if (state.id != ArtistID::Invalid)
                            break;
                    }
                }
            }
            ImGui::TableNextColumn();
            SourceSelection();
            ImGui::EndTable();
        }

        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable /*| ImGuiTableFlags_SortTristate*/
            | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

        enum IDs
        {
            kCheck,
            kID,
            kHandle,
            kDescription,
            kSource,
            kNumIDs
        };

        if (ImGui::BeginTable("artists", kNumIDs, flags, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
        {
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_NoResize, 0.0f, kCheck);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_NoResize, 0.0f, kID);
            ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthStretch, 0.0f, kHandle);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.0f, kDescription);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 0.0f, kSource);
            ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(m_importArtists.artists.NumItems<int32_t>());
            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    auto& artist = m_importArtists.artists[rowIdx];
                    auto& state = m_importArtists.states[rowIdx];
                    // Display a data item
                    ImGui::PushID(artist.id.value);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable("##select", state.isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap))
                    {
                        if (ImGui::GetIO().KeyShift)
                        {
                            if (m_importArtists.lastSelected < 0)
                                m_importArtists.lastSelected = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                for (auto& s : m_importArtists.states)
                                    s.isSelected = false;
                            }
                            auto start = rowIdx;
                            auto end = m_importArtists.lastSelected;
                            if (start > end)
                                std::swap(start, end);
                            for (; start <= end; start++)
                                m_importArtists.states[start].isSelected = true;
                            state.isSelected = true;
                        }
                        else
                        {
                            m_importArtists.lastSelected = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                for (auto& s : m_importArtists.states)
                                    s.isSelected = false;
                            }
                            state.isSelected = !state.isSelected;
                        }
                    }
                    if (ImGui::BeginPopupContextItem("Artist Popup"))
                    {
                        if (ImGui::Selectable("Check all"))
                        {
                            for (auto& s : m_importArtists.states)
                                s.isImported = true;
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Check none"))
                        {
                            for (auto& s : m_importArtists.states)
                                s.isImported = false;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::Separator();
                        auto canReCheck = m_importArtists.states.FindIf([](auto& state) { return state.isSelected; }) != nullptr;
                        ImGui::BeginDisabled(!canReCheck);
                        if (ImGui::Selectable("Check selected"))
                        {
                            for (auto& s : m_importArtists.states)
                            {
                                if (s.isSelected)
                                    s.isImported = true;
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Uncheck selected"))
                        {
                            for (auto& s : m_importArtists.states)
                            {
                                if (s.isSelected)
                                    s.isImported = false;
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Invert selected"))
                        {
                            for (auto& s : m_importArtists.states)
                            {
                                if (s.isSelected)
                                    s.isImported = !s.isImported;
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndDisabled();

                        ImGui::EndPopup();
                    }
                    ImGui::SameLine(0.0f, 0.0f);//no spacing
                    ImGui::Checkbox("###CheckedArtist", &state.isImported);
                    ImGui::TableNextColumn();
                    if (state.id != ArtistID::Invalid)
                    {
                        ImGui::Text("%04X", uint32_t(state.id));
                        if (ImGui::IsItemHovered())
                            m_db[state.id]->Tooltip();
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(artist.name.c_str());
                    if (ImGui::IsItemHovered())
                        ImGui::Tooltip(artist.name.c_str());
                    ImGui::TableNextColumn();
                    size_t s = artist.description.find_first_of('\n');
                    if (s != static_cast<size_t>(-1))
                        ImGui::TextUnformatted(artist.description.c_str(), artist.description.c_str() + s);
                    else
                        ImGui::TextUnformatted(artist.description.c_str());
                    if (ImGui::IsItemHovered())
                        ImGui::Tooltip(artist.description.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(SourceID::sourceNames[artist.id.sourceId]);
                    if (ImGui::IsItemHovered())
                        ImGui::Tooltip(SourceID::sourceNames[artist.id.sourceId]);
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();

            auto canImport = m_importArtists.states.FindIf([](auto& state) { return state.isImported; }) != nullptr;
            ImGui::BeginDisabled(!canImport);
            if (ImGui::Button("Import", ImVec2(-1.0f, 0.0f)))
            {
                m_imports = {};
                for (uint32_t i = 0; i < m_importArtists.artists.NumItems(); i++)
                {
                    if (m_importArtists.states[i].isImported)
                        ImportArtist(m_importArtists.artists[i].id);
                }
                m_imports.isOpened = &m_importArtists.isOpened;
            }
            ImGui::EndDisabled();
        }
    }

    void Library::ImportArtist(SourceID artistId)
    {
        m_sources[artistId.sourceId]->ImportArtist(artistId, m_imports.sourceResults);
    }

    void Library::ProcessImports()
    {
        bool isDirty = false;
        if (m_imports.selected.IsEmpty())
        {
            m_imports.selected.Resize(m_imports.sourceResults.songs.NumItems());
            std::fill(m_imports.selected.begin(), m_imports.selected.end(), false);
            m_imports.sortedSongs.Resize(m_imports.sourceResults.songs.NumItems());
            for (uint32_t i = 0, e = m_imports.sortedSongs.NumItems(); i < e; i++)
                m_imports.sortedSongs[i] = i;
            isDirty = true;
        }

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
            | ImGuiTableFlags_RowBg | /*ImGuiTableFlags_NoBordersInBody | */ImGuiTableFlags_ScrollY;

        enum IDs
        {
            kCheck,
            kState,
            kTitle,
            kArtist,
            kType,
            kSource,
            kNumIDs
        };

        if (ImGui::BeginTable("songs", kNumIDs, flags, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
        {
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 0.0f, kCheck);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 0.0f, kState);
            ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.0f, kTitle);
            ImGui::TableSetupColumn("Artist", ImGuiTableColumnFlags_WidthStretch, 0.0f, kArtist);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.0f, kType);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 0.0f, kSource);
            ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
            ImGui::TableHeadersRow();

            auto* sortsSpecs = ImGui::TableGetSortSpecs();
            if (sortsSpecs && (sortsSpecs->SpecsDirty || isDirty))
            {
                if (m_imports.sourceResults.songs.NumItems() > 1)
                {
                    std::sort(m_imports.sortedSongs.begin(), m_imports.sortedSongs.end(), [this, sortsSpecs](uint32_t l, uint32_t r)
                    {
                        const auto& lSong = m_imports.sourceResults.songs[l];
                        const auto& rSong = m_imports.sourceResults.songs[r];
                        for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                        {
                            auto& sortSpec = sortsSpecs->Specs[i];
                            int64_t delta = 0;
                            switch (sortSpec.ColumnUserID)
                            {
                            case kCheck:
                                delta = m_imports.sourceResults.states[l].IsChecked() - m_imports.sourceResults.states[r].IsChecked();
                                break;
                            case kState:
                                delta = m_imports.sourceResults.states[l].GetSongStatus() - m_imports.sourceResults.states[r].GetSongStatus();
                                break;
                            case kTitle:
                                delta = _stricmp(lSong->name.Items(), rSong->name.Items());
                                break;
                            case kArtist:
                            {
                                static const char* const empty = "";
                                const char* lArtist = lSong->artistIds.IsEmpty() ? empty : m_imports.sourceResults.artists[static_cast<uint32_t>(lSong->artistIds[0])]->handles[0].Items();
                                const char* rArtist = rSong->artistIds.IsEmpty() ? empty : m_imports.sourceResults.artists[static_cast<uint32_t>(rSong->artistIds[0])]->handles[0].Items();
                                delta = _stricmp(lArtist, rArtist);
                            }
                            break;
                            case kType:
                                delta = strcmp(lSong->type.GetExtension(), rSong->type.GetExtension());
                                break;
                            case kSource:
                                delta = strcmp(SourceID::sourceNames[lSong->sourceIds[0].sourceId], SourceID::sourceNames[rSong->sourceIds[0].sourceId]);
                                break;
                            }

                            if (delta)
                                return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                        }
                        return l < r;
                    });
                }
                sortsSpecs->SpecsDirty = false;
            }

            ImGuiListClipper clipper;
            clipper.Begin(m_imports.sourceResults.songs.NumItems<int32_t>());
            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    auto songIndex = m_imports.sortedSongs[rowIdx];
                    auto song = m_imports.sourceResults.songs[songIndex];
                    auto& songState = m_imports.sourceResults.states[songIndex];
                    ImGui::PushID(songIndex);

                    ImGui::TableNextColumn();
                    bool isSelected = m_imports.selected[songIndex];
                    if (ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap))
                    {
                        if (ImGui::GetIO().KeyShift)
                        {
                            if (m_imports.lastSelected < 0)
                                m_imports.lastSelected = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                                std::fill(m_imports.selected.begin(), m_imports.selected.end(), false);
                            auto start = rowIdx;
                            auto end = m_imports.lastSelected;
                            if (start > end)
                                std::swap(start, end);
                            for (; start <= end; start++)
                                m_imports.selected[m_imports.sortedSongs[start]] = true;
                            isSelected = true;
                        }
                        else
                        {
                            m_imports.lastSelected = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                                std::fill(m_imports.selected.begin(), m_imports.selected.end(), false);
                            isSelected = !isSelected;
                            m_imports.selected[songIndex] = isSelected;
                        }
                    }
                    if (ImGui::BeginPopupContextItem("Song Popup"))
                    {
                        if (ImGui::Selectable("Reset to default"))
                        {
                            for (auto& state : m_imports.sourceResults.states)
                                state.SetChecked(state.IsNew());
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Check all"))
                        {
                            for (auto& state : m_imports.sourceResults.states)
                                state.SetChecked(!state.IsOwned());
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Check none"))
                        {
                            for (auto& state : m_imports.sourceResults.states)
                                state.SetChecked(false);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::Separator();
                        auto canReCheck = std::find(m_imports.selected.begin(), m_imports.selected.end(), true) != m_imports.selected.end();
                        ImGui::BeginDisabled(!canReCheck);
                        if (ImGui::Selectable("Check selected"))
                        {
                            for (uint32_t i = 0; i < m_imports.selected.NumItems(); i++)
                            {
                                if (m_imports.selected[i])
                                {
                                    auto& state = m_imports.sourceResults.states[i];
                                    state.SetChecked(!state.IsOwned());
                                }
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Uncheck selected"))
                        {
                            for (uint32_t i = 0; i < m_imports.selected.NumItems(); i++)
                            {
                                if (m_imports.selected[i])
                                {
                                    auto& state = m_imports.sourceResults.states[i];
                                    state.SetChecked(false);
                                }
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Invert selected"))
                        {
                            for (uint32_t i = 0; i < m_imports.selected.NumItems(); i++)
                            {
                                if (m_imports.selected[i])
                                {
                                    auto& state = m_imports.sourceResults.states[i];
                                    state.SetChecked(!state.IsOwned() && !state.IsChecked());
                                }
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndDisabled();

                        ImGui::EndPopup();
                    }

                    ImGui::SameLine(0.0f, 0.0f);//no spacing
                    ImGui::BeginDisabled(songState.IsOwned());
                    bool isChecked = songState.IsChecked();
                    if (ImGui::Checkbox("###CheckedSong", &isChecked))
                        songState.SetChecked(isChecked);
                    ImGui::TableNextColumn();
                    static const char* const statusNames[] = { "New", "Merged", "Discarded", "Owned" };
                    ImGui::TextUnformatted(statusNames[songState.GetSongStatus()]);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(song->name.Items());
                    if (ImGui::IsItemHovered())
                        ImGui::Tooltip(song->name.Items());
                    ImGui::TableNextColumn();
                    if (song->artistIds.IsNotEmpty())
                    {
                        ImGui::BeginGroup();
                        for (uint16_t i = 0; i < song->artistIds.NumItems<uint16_t>(); i++)
                        {
                            if (i != 0)
                            {
                                ImGui::SameLine(0.0f, 0.0f);
                                ImGui::TextUnformatted(", ");
                                ImGui::SameLine(0.0f, 0.0f);
                            }
                            ImGui::TextUnformatted(m_imports.sourceResults.artists[static_cast<uint32_t>(song->artistIds[i])]->handles[0].Items());
                        }
                        ImGui::EndGroup();
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            for (uint16_t i = 0; i < song->artistIds.NumItems<uint16_t>(); i++)
                                ImGui::TextUnformatted(m_imports.sourceResults.artists[static_cast<uint32_t>(song->artistIds[i])]->handles[0].Items());
                            ImGui::EndTooltip();
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(song->type.GetExtension());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(SourceID::sourceNames[song->sourceIds[0].sourceId]);
                    if (ImGui::IsItemHovered())
                        ImGui::Tooltip(SourceID::sourceNames[song->sourceIds[0].sourceId]);

                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();

            uint32_t numChecked = 0;
            for (auto& state : m_imports.sourceResults.states)
            {
                if (state.IsChecked())
                    numChecked++;
            }
            ImGui::BeginDisabled(numChecked == 0);
            char txt[64] = "Nothing to import###Process";
            if (numChecked > 0)
                sprintf(txt, "%d %s to import###Process", numChecked, numChecked == 1 ? "song" : "songs");
            if (ImGui::Button(txt, ImVec2(-1.0f, 0.0f)))
            {
                auto databaseDay = uint16_t((std::chrono::sys_days(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())) - std::chrono::sys_days(std::chrono::days(Core::kReferenceDate))).count());

                // first, update the fetch time of the imported artist if they already exist in the library
                for (auto importedArtistId : m_imports.sourceResults.importedArtists)
                {
                    bool isDone = false;
                    for (auto newArtist : m_imports.sourceResults.artists)
                    {
                        if (newArtist->sources[0].id == importedArtistId)
                        {
                            // check if the artist is in the library
                            for (Artist* artist : m_db.Artists())
                            {
                                for (uint16_t i = 0; i < artist->NumSources(); i++)
                                {
                                    // found it, update
                                    if (artist->GetSource(i).id == newArtist->sources[0].id)
                                    {
                                        artist->Edit()->sources[i].timeFetch = newArtist->sources[0].timeFetch;
                                        isDone = true;
                                        break;
                                    }
                                }
                                if (isDone)
                                    break;
                            }
                        }
                        if (isDone)
                            break;
                    }
                }

                // second, process the songs
                for (uint32_t songIndex = 0; songIndex < m_imports.sourceResults.songs.NumItems(); songIndex++)
                {
                    auto& newSong = m_imports.sourceResults.songs[songIndex];
                    if (m_imports.sourceResults.states[songIndex].IsChecked())
                    {
                        for (uint32_t artistIndex = 0; artistIndex < newSong->artistIds.NumItems();)
                        {
                            auto& newArtist = m_imports.sourceResults.artists[static_cast<int>(newSong->artistIds[artistIndex])];
                            // check if the artist is already processed
                            if (newArtist->id == ArtistID::Invalid)
                            {
                                // check if the artist is in the library
                                for (Artist* artist : m_db.Artists())
                                {
                                    for (auto& source : artist->Sources())
                                    {
                                        // already imported, replace the new one by the old one
                                        if (source.id == newArtist->sources[0].id)
                                        {
                                            newArtist = artist->Edit();
                                            break;
                                        }
                                    }
                                    if (newArtist->id != ArtistID::Invalid)
                                        break;
                                }
                                // no artist found, add it to our library
                                if (newArtist->id == ArtistID::Invalid)
                                {
                                    m_db.AddArtist(newArtist);
                                    // if it's not one of the imported artists, then reset it's fetched time
                                    auto holdTimeFetch = newArtist->sources[0].timeFetch;
                                    newArtist->sources[0].timeFetch = 0;
                                    for (auto importedArtistId : m_imports.sourceResults.importedArtists)
                                    {
                                        if (importedArtistId == newArtist->sources[0].id)
                                        {
                                            newArtist->sources[0].timeFetch = holdTimeFetch;
                                            break;
                                        }
                                    }
                                    m_sources[newSong->sourceIds[0].sourceId]->OnArtistUpdate(newArtist);
                                }
                            }

                            // remap the artist id in the song
                            auto artistEnd = newSong->artistIds.begin() + artistIndex;
                            if (artistIndex > 0 && std::find(newSong->artistIds.begin(), artistEnd, newArtist->id) != artistEnd)
                            {
                                // remove duplicated artist
                                newSong->artistIds.RemoveAt(artistIndex);
                            }
                            else
                            {
                                newArtist->numSongs++;
                                newSong->artistIds[artistIndex] = newArtist->id;
                                artistIndex++;
                            }
                        }

                        // if the song was merged, unref it it from the previous song
                        if (m_imports.sourceResults.states[songIndex].GetSongStatus() == SourceResults::kMerged)
                        {
                            auto* oldSong = m_db[newSong->id]->Edit();
                            oldSong->sourceIds.Remove(newSong->sourceIds[0]);
                        }

                        newSong->databaseDay = databaseDay;

                        auto* dbNewSong = m_db.AddSong(newSong);
                        m_sources[newSong->sourceIds[0].sourceId]->OnSongUpdate(dbNewSong);
                    }
                }

                // third, save states, clear and close
                m_db.Raise(Database::Flag::kSaveSongs | Database::Flag::kSaveArtists);

                *m_imports.isOpened = false;
                m_imports = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }
    }

    void Library::Load()
    {
        auto file = io::File::OpenForRead(ms_songsFilename);
        if (file.IsValid())
        {
            if (file.Read<uint32_t>() != kMusicFileStamp)
            {
                assert(0 && "file read error");
                return;
            }
            if (m_db.LoadSongs(file) != Status::kOk)
            {
                assert(0 && "file read error");
                return;
            }
        }

        file = io::File::OpenForRead(ms_artistsFilename);
        if (file.IsValid())
        {
            if (file.Read<uint32_t>() != kMusicFileStamp)
            {
                assert(0 && "file read error");
                return;
            }
            if (m_db.LoadArtists(file) != Status::kOk)
            {
                assert(0 && "file read error");
                return;
            }
        }

        for (auto* source : m_sources)
            source->Load();
    }

    void Library::Save()
    {
        auto saveFlags = m_db.Fetch();

        if (saveFlags.IsEnabled(Database::Flag::kSaveSongs))
        {
            if (!m_hasSongsBackup)
            {
                std::string backupFileame = ms_songsFilename;
                backupFileame += ".bak";
                io::File::Copy(ms_songsFilename, backupFileame.c_str());
                m_hasSongsBackup = true;
            }
            auto file = io::File::OpenForWrite(ms_songsFilename);
            if (file.IsValid())
            {
                file.Write(kMusicFileStamp);
                m_db.SaveSongs(file);
            }
        }

        if (saveFlags.IsEnabled(Database::Flag::kSaveArtists))
        {
            if (!m_hasArtistsBackup)
            {
                std::string backupFileame = ms_artistsFilename;
                backupFileame += ".bak";
                io::File::Copy(ms_artistsFilename, backupFileame.c_str());
                m_hasArtistsBackup = true;
            }
            auto file = io::File::OpenForWrite(ms_artistsFilename);
            if (file.IsValid())
            {
                // todo: remove validation
                for (auto* artist : m_db.Artists())
                    ValidateArtist(artist);

                file.Write(kMusicFileStamp);
                m_db.SaveArtists(file);
            }
        }

        for (auto source : m_sources)
            source->Save();
    }

    void Library::ValidateArtist(const Artist* const artist) const
    {
        uint32_t numSongs = 0;
        for (auto* song : m_db.Songs())
        {
            for (auto songArtistId : song->ArtistIds())
                if (songArtistId == artist->GetId())
                    numSongs++;
        }

        assert(numSongs == artist->NumSongs());
    }
}
// namespace rePlayer