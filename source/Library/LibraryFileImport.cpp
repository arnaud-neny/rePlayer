// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <Imgui.h>
#include <IO/StreamFile.h>

// rePlayer
#include <Library/LibraryDatabase.h>
#include <Library/LibrarySongsUI.h>
#include <IO/StreamArchive.h>
#include <IO/StreamArchiveRaw.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <Replays/Replay.h>
#include <UI/FileSelector.h>

#include "LibraryFileImport.h"

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

// zlib
#include <zlib.h>

// stl
#include <chrono>

namespace rePlayer
{
    void Library::FileImport::Scan(Array<std::filesystem::path>&& files)
    {
        Core::GetLibrary().m_isBusy = true;
        Core::AddJob([this, files = std::move(files)]()
        {
            auto& library = Core::GetLibrary();
            auto& replays = Core::GetReplays();

            for (auto& path : files)
            {
                auto filename = path.u8string();
                SmartPtr<io::Stream> stream = io::StreamFile::Create(reinterpret_cast<const char*>(filename.c_str()));
                if (stream.IsInvalid())
                {
                    Core::FromJob([filename]()
                    {
                        Log::Error("FileImport: Can't open \"%s\"\n", reinterpret_cast<const char*>(filename.c_str()));
                    });
                    continue;
                }

                auto fileSize = stream->GetSize();
                if (fileSize == 0)
                {
                    Core::FromJob([filename]()
                    {
                        Log::Warning("FileImport: \"%s\" is empty\n", reinterpret_cast<const char*>(filename.c_str()));
                    });
                    continue;
                }

                bool isArchive = false;
                auto playables = replays.Enumerate(stream, GetMediaType(stream->GetName()));
                if (playables[0] == eReplay::Unknown)
                {
                    SmartPtr<io::Stream> streamArchive = StreamArchive::Create(stream, true);
                    if (streamArchive.IsInvalid())
                        streamArchive = StreamArchiveRaw::Create(stream);
                    if (streamArchive.IsValid())
                    {
                        stream = streamArchive;
                        playables = replays.Enumerate(stream, GetMediaType(stream->GetName()));
                        isArchive = true;
                    }
                }

                for (;;)
                {
                    m_sortedEntries.Add(m_entries.NumItems());
                    auto* newEntry = m_entries.Push();
                    newEntry->path = path;
                    newEntry->name = reinterpret_cast<const char*>(std::filesystem::relative(path, std::filesystem::path(library.m_lastFileDialogPath.As<std::string>())).u8string().c_str());
                    if (newEntry->name.empty())
                        newEntry->name = reinterpret_cast<const char*>(filename.c_str());
                    if (isArchive)
                    {
                        newEntry->name += ':';
                        newEntry->name += stream->GetName();
                    }

                    for (uint32_t i = 0; i < Entry::kMaxReplays; i++)
                    {
                        if (playables[i] != eReplay::Unknown)
                            newEntry->replays[i] = uint8_t(playables[i]);
                        else
                            break;
                    }
                    newEntry->currentReplay = newEntry->replays[0];
                    newEntry->isChecked = eReplay(newEntry->currentReplay) != eReplay::Unknown;
                    newEntry->isArchive = isArchive;

                    stream = stream->Next(true);
                    if (stream.IsValid())
                        playables = replays.Enumerate(stream, GetMediaType(stream->GetName()));
                    else
                        break;
                }
            }
            Core::FromJob([this]()
            {
                Core::GetLibrary().m_isBusy = false;
                m_isScanning = false;
                m_isImporting = true;
            });
        });
    }

    Library::FileImport* Library::FileImport::Display()
    {
        if (m_isScanning)
            return this;

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
            | ImGuiTableFlags_RowBg | /*ImGuiTableFlags_NoBordersInBody | */ImGuiTableFlags_ScrollY;

        enum IDs
        {
            kCheck,
            kFile,
            kReplay,
            kNumIDs
        };

        if (ImGui::BeginTable("files", kNumIDs, flags, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
        {
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 0.0f, kCheck);
            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 0.0f, kFile);
            ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_WidthStretch, 0.0f, kReplay);
            ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
            ImGui::TableHeadersRow();

            auto& replays = Core::GetReplays();

            auto* sortsSpecs = ImGui::TableGetSortSpecs();
            if (sortsSpecs && (sortsSpecs->SpecsDirty /*|| isDirty*/))
            {
                if (m_entries.NumItems() > 1)
                {
                    std::sort(m_sortedEntries.begin(), m_sortedEntries.end(), [this, sortsSpecs, &replays](uint32_t l, uint32_t r)
                    {
                        auto& lEntry = m_entries[l];
                        auto& rEntry = m_entries[r];
                        for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                        {
                            auto& sortSpec = sortsSpecs->Specs[i];
                            int64_t delta = 0;
                            switch (sortSpec.ColumnUserID)
                            {
                            case kCheck:
                                delta = rEntry.isChecked - lEntry.isChecked;
                                break;
                            case kFile:
                                delta = CompareStringMixed(lEntry.name.c_str(), rEntry.name.c_str());
                                break;
                            case kReplay:
                                delta = strcmp(replays.GetName(eReplay(lEntry.currentReplay)), replays.GetName(eReplay(rEntry.currentReplay)));
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
            clipper.Begin(m_sortedEntries.NumItems<int32_t>());
            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    auto entryIndex = m_sortedEntries[rowIdx];
                    ImGui::PushID(entryIndex);

                    ImGui::TableNextColumn();
                    bool isSelected = m_entries[entryIndex].isSelected;
                    if (ImGui::Selectable("##select", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                    {
                        if (ImGui::GetIO().KeyShift)
                        {
                            if (m_lastSelected < 0)
                                m_lastSelected = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                for (auto& entry : m_entries)
                                    entry.isSelected = false;
                            }
                            auto start = rowIdx;
                            auto end = m_lastSelected;
                            if (start > end)
                                std::swap(start, end);
                            for (; start <= end; start++)
                                m_entries[m_sortedEntries[start]].isSelected = true;
                            isSelected = true;
                        }
                        else
                        {
                            m_lastSelected = rowIdx;
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                for (auto& entry : m_entries)
                                    entry.isSelected = false;
                            }
                            isSelected = !isSelected;
                            m_entries[entryIndex].isSelected = isSelected;
                        }
                    }
                    if (ImGui::BeginPopupContextItem("File Popup"))
                    {
                        if (ImGui::Selectable("Check all"))
                        {
                            for (auto& entry : m_entries)
                                entry.isChecked = true;
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Check none"))
                        {
                            for (auto& entry : m_entries)
                                entry.isChecked = false;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::Separator();
                        if (ImGui::Selectable("Check selected"))
                        {
                            for (auto& entry : m_entries)
                            {
                                if (entry.isSelected)
                                    entry.isChecked = true;
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Uncheck selected"))
                        {
                            for (auto& entry : m_entries)
                            {
                                if (entry.isSelected)
                                    entry.isChecked = false;
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Selectable("Invert selected"))
                        {
                            for (auto& entry : m_entries)
                            {
                                if (entry.isSelected)
                                    entry.isChecked = ~entry.isChecked;
                            }
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }
                    ImGui::SameLine(0.0f, 0.0f);//no spacing
                    bool isChecked = m_entries[entryIndex].isChecked;
                    if (ImGui::Checkbox("##Checked", &isChecked))
                        m_entries[entryIndex].isChecked = isChecked;

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(m_entries[entryIndex].name.c_str(), m_entries[entryIndex].name.c_str() + m_entries[entryIndex].name.size());

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo("##ReplayCB", replays.GetName(eReplay(m_entries[entryIndex].currentReplay)), ImGuiComboFlags_None))
                    {
                        auto* replaysEnd = m_entries[entryIndex].replays;
                        for (uint8_t replay : m_entries[entryIndex].replays)
                        {
                            if (eReplay(replay) == eReplay::Unknown)
                                break;
                            ImGui::PushID(replay);
                            const bool isItemSelected = replay == m_entries[entryIndex].currentReplay;
                            if (ImGui::Selectable(replays.GetName(eReplay(replay)), isItemSelected))
                                m_entries[entryIndex].currentReplay = replay;
                            ImGui::PopID();
                            replaysEnd++;
                        }

                        if (eReplay(m_entries[entryIndex].replays[0]) != eReplay::Unknown)
                            ImGui::Separator();

                        for (uint32_t i = 0; i < uint32_t(eReplay::Count); ++i)
                        {
                            bool isReplayable = std::find(m_entries[entryIndex].replays, replaysEnd, uint8_t(i)) != replaysEnd;
                            if (!isReplayable)
                            {
                                ImGui::PushID(i);
                                const bool isItemSelected = i == m_entries[entryIndex].currentReplay;
                                if (ImGui::Selectable(replays.GetName(eReplay(i)), isItemSelected))
                                    m_entries[entryIndex].currentReplay = uint8_t(i);
                                ImGui::PopID();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();

            static const char* artistModes[] = {
                "No Artist",
                "New Artist",
                "Root Path",
                "Parent Path"
            };
            ImGui::SetNextItemWidth(ImGui::CalcTextSize(artistModes[3]).x + 32.0f * ImGui::GetIO().FontGlobalScale);
            ImGui::Combo("##ArtistCB", reinterpret_cast<int*>(&m_artistMode), artistModes, NumItemsOf(artistModes));
            ImGui::SameLine();

            uint32_t numChecked = 0;
            for (auto& entry : m_entries)
            {
                if (entry.isChecked)
                    numChecked++;
            }
            ImGui::BeginDisabled(numChecked == 0);
            char txt[64] = "Nothing to import###Process";
            if (numChecked > 0)
                sprintf(txt, "%u %s to import###Process", numChecked, numChecked == 1 ? "song" : "songs");
            if (ImGui::Button(txt, ImVec2(-1.0f, 0.0f)))
            {
                Core::GetLibrary().m_db.Freeze();
                Core::GetLibrary().m_isBusy = true;
                Core::AddJob([this]()
                {
                    auto& library = Core::GetLibrary();
                    auto& replays = Core::GetReplays();
                    auto databaseDay = uint16_t((std::chrono::sys_days(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())) - std::chrono::sys_days(std::chrono::days(Core::kReferenceDate))).count());

                    Array<Artist*> artists;

                    for (auto& entry : m_entries)
                    {
                        if (!entry.isChecked)
                            continue;

                        auto filename = entry.path.u8string();
                        SmartPtr<io::Stream> stream = io::StreamFile::Create(reinterpret_cast<const char*>(filename.c_str()));
                        if (stream.IsInvalid())
                        {
                            Core::FromJob([filename]()
                            {
                                Log::Error("FileImport: Can't open \"%s\"\n", reinterpret_cast<const char*>(filename.c_str()));
                            });
                            continue;
                        }

                        auto fileSize = stream->GetSize();
                        if (fileSize == 0)
                        {
                            Core::FromJob([filename]()
                            {
                                Log::Warning("FileImport: \"%s\" is empty\n", reinterpret_cast<const char*>(filename.c_str()));
                            });
                            continue;
                        }

                        if (entry.isArchive)
                        {
                            auto name = entry.name.substr(entry.name.find_last_of(':') + 1);

                            SmartPtr<io::Stream> streamArchive = StreamArchive::Create(stream, true);
                            if (streamArchive.IsInvalid())
                                streamArchive = StreamArchiveRaw::Create(stream);
                            else while (streamArchive->GetName() != name)
                                streamArchive = streamArchive->Next(true);
                            if (streamArchive.IsValid())
                                stream = std::move(streamArchive);
                        }

                        auto* song = new SongSheet;
                        auto* dbSong = library.m_db.AddSong(song);
                        song->name = entry.name;
                        auto extPos = entry.name.find_last_of('.');
                        if (extPos != entry.name.npos && MediaType(entry.name.c_str() + extPos + 1, eReplay::Unknown).ext != eExtension::Unknown)
                            song->name.String().resize(extPos);
                        song->databaseDay = databaseDay;
                        song->sourceIds.Add(SourceID(SourceID::FileImportID, 0));

                        switch (m_artistMode)
                        {
                        case ArtistMode::kNoArtist:
                            break;
                        case ArtistMode::kNewArtist:
                            if (artists.IsEmpty())
                            {
                                auto* artist = new ArtistSheet;
                                artist->handles.Add("New Imported Artist");
                                artists.Add(library.m_db.AddArtist(artist));
                            }
                            song->artistIds.Add(artists.Last()->id);
                            artists.Last()->Edit()->numSongs++;
                            break;
                        case ArtistMode::kRootPath:
                            if (artists.IsEmpty())
                            {
                                auto path = std::filesystem::path(library.m_lastFileDialogPath.As<std::string>());
                                auto* artist = new ArtistSheet;
                                artist->handles.Add(reinterpret_cast<const char*>(std::filesystem::relative(path, path.parent_path()).u8string().c_str()));
                                artists.Add(library.m_db.AddArtist(artist));
                            }
                            song->artistIds.Add(artists.Last()->id);
                            artists.Last()->Edit()->numSongs++;
                            break;
                        case ArtistMode::kParentPath:
                            {
                                auto path = entry.path;
                                path.remove_filename();
                                std::string artistName = reinterpret_cast<const char*>(std::filesystem::relative(path, std::filesystem::canonical(path / "..")).u8string().c_str());
                                Artist* artist = nullptr;
                                for (auto* a : artists)
                                {
                                    if (a->GetHandle(0) == artistName)
                                    {
                                        artist = a;
                                        break;
                                    }
                                }
                                if (artist == nullptr)
                                {
                                    auto* artistSheet = new ArtistSheet;
                                    artistSheet->handles.Add(artistName.c_str());
                                    artist = library.m_db.AddArtist(artistSheet);
                                    artists.Add(artist);
                                }
                                song->artistIds.Add(artist->id);
                                artist->Edit()->numSongs++;
                            }
                            break;
                        }

                        song->type = GetMediaType(stream->GetName());
                        song->type.replay = eReplay(entry.currentReplay);
                        if (auto* replay = replays.Load(stream, song->metadata.Container(), song->type))
                        {
                            song->type = replay->GetMediaType();
                            auto numSubsongs = replay->GetNumSubsongs();
                            song->lastSubsongIndex = uint16_t(numSubsongs - 1);
                            song->subsongs.Resize(numSubsongs);
                            for (uint32_t i = 0; i < numSubsongs; i++)
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
                            delete replay;
                        }

                        auto filenames = stream->GetFilenames();
                        if (filenames.NumItems() == 1)
                        {
                            auto fileData = stream->Read();

                            song->fileSize = uint32_t(fileData.Size());

                            auto fileCrc = crc32(0L, Z_NULL, 0);
                            song->fileCrc = crc32_z(fileCrc, fileData.Items(), fileData.Size());

                            auto file = io::File::OpenForWrite(library.m_db.GetFullpath(dbSong, &artists).c_str());
                            file.Write(fileData.Items(), fileData.Size());
                        }
                        else
                        {
                            song->subsongs[0].isArchive = true;
                            song->subsongs[0].isPackage = true;

                            struct ArchiveBuffer : public Array<uint8_t>
                            {
                                static int ArchiveOpen(struct archive*, void*)
                                {
                                    return ARCHIVE_OK;
                                }

                                static int ArchiveClose(struct archive*, void*)
                                {
                                    return ARCHIVE_OK;
                                }

                                static int ArchiveFree(struct archive*, void*)
                                {
                                    return ARCHIVE_OK;
                                }

                                static la_ssize_t ArchiveWrite(struct archive*, void* _client_data, const void* _buffer, size_t _length)
                                {
                                    reinterpret_cast<ArchiveBuffer*>(_client_data)->Add(reinterpret_cast<const uint8_t*>(_buffer), uint32_t(_length));
                                    return _length;
                                }
                            } archiveBuffer;

                            auto* archive = archive_write_new();
                            archive_write_set_format_7zip(archive);
                            archive_write_open2(archive, &archiveBuffer, archiveBuffer.ArchiveOpen, ArchiveBuffer::ArchiveWrite, archiveBuffer.ArchiveClose, archiveBuffer.ArchiveFree);
                            auto archiveEntry = archive_entry_new();

                            auto parentPath = std::filesystem::path(filenames[0]).parent_path();
                            for (auto& f : filenames)
                            {
                                stream = stream->Open(f);//io::StreamFile::Create(f);
                                auto fileData = stream->Read();

                                archive_entry_set_pathname(archiveEntry, reinterpret_cast<const char*>(std::filesystem::proximate(f, parentPath).u8string().c_str()));
                                archive_entry_set_size(archiveEntry, fileData.Size());
                                archive_entry_set_filetype(archiveEntry, AE_IFREG);
                                archive_entry_set_perm(archiveEntry, 0644);
                                archive_write_header(archive, archiveEntry);
                                archive_write_data(archive, fileData.Items(), fileData.Size());
                                archive_entry_clear(archiveEntry);
                            }

                            archive_write_free(archive);
                            archive_entry_free(archiveEntry);

                            song->fileSize = uint32_t(archiveBuffer.Size());

                            auto fileCrc = crc32(0L, Z_NULL, 0);
                            song->fileCrc = crc32_z(fileCrc, archiveBuffer.Items(), archiveBuffer.Size<size_t>());

                            auto file = io::File::OpenForWrite(library.m_db.GetFullpath(dbSong, &artists).c_str());
                            file.Write(archiveBuffer.Items(), archiveBuffer.Size());
                        }
                    }

                    library.m_db.Raise(Database::Flag::kSaveSongs);

                    Core::FromJob([this]()
                    {
                        Core::GetLibrary().m_db.UnFreeze();
                        Core::GetLibrary().m_isBusy = false;
                        Core::Unlock();
                        m_isImporting = false;
                    });
                });
            }
            ImGui::EndDisabled();
        }

        if (!m_isImporting)
        {
            Core::Unlock();
            delete this;
            return nullptr;
        }
        return this;
    }

    MediaType Library::FileImport::GetMediaType(const std::filesystem::path& path)
    {
        auto guessedExtension = path.has_extension() ? path.extension().u8string().substr(1) : std::u8string();
        const auto pathExtension = guessedExtension;
        const auto pathStem = path.stem().u8string();

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
        return Core::GetReplays().Find(reinterpret_cast<const char*>(guessedExtension.c_str()));
    }
}
// namespace rePlayer