#include "SongEditor.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <Helpers/CommandBuffer.inl.h>
#include <Imgui.h>
#include <Imgui/imgui_internal.h>
#include <IO/File.h>
#include <IO/Stream.h>

#include <Database/SongEndEditor.h>
#include <Deck/Deck.h>
#include <Library/Library.h>
#include <Library/LibraryDatabase.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <Replays/ReplayContexts.h>

#include <algorithm>
#include <chrono>

namespace rePlayer
{
    SongEditor::SongEditor()
        : Window("SongEditor", ImGuiWindowFlags_NoCollapse)
    {}

    SongEditor::~SongEditor()
    {
        delete m_artistFilter;
    }


    void SongEditor::OnSongSelected(MusicID musicId)
    {
        if (m_musicId.subsongId.songId != musicId.subsongId.songId || m_musicId.databaseId != musicId.databaseId)
        {
            musicId.GetSong()->CopyTo(&m_song.original);
            musicId.GetSong()->CopyTo(&m_song.edited);
            m_playables[0] = eReplay::Unknown;
            m_selectedSubsong = 0;
        }
        m_musicId = musicId;
    }

    std::string SongEditor::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(0.0f, 300.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(-FLT_MIN, 300.0f), ImVec2(-FLT_MIN, FLT_MAX));
        return "Song Editor";
    }

    void SongEditor::OnDisplay()
    {
        m_busyColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        ImGui::BeginDisabled(m_isBusy);
        ImGui::BeginBusy(m_isBusy);

        auto window = ImGui::GetCurrentWindow();
        window->AutoFitFramesX = 2;

        // check if the song has not been updated outside the editor
        Song* currentSong = nullptr;
        if (m_musicId.IsValid())
        {
            currentSong = m_musicId.GetSong();
            // we don't want to loose everything we edited, so just update the changes
            if (m_song.original.type != currentSong->GetType())
            {
                m_song.original.type = currentSong->GetType();
                m_song.edited.type = m_song.original.type;
            }
            if (m_song.original.name != currentSong->GetName())
            {
                m_song.original.name.String() = currentSong->GetName();
                m_song.edited.name = m_song.original.name;
            }
            if (m_song.original.releaseYear != currentSong->GetReleaseYear())
            {
                m_song.original.releaseYear = currentSong->GetReleaseYear();
                m_song.edited.releaseYear = m_song.original.releaseYear;
            }
            if (m_song.original.artistIds != currentSong->ArtistIds())
            {
                m_song.original.artistIds = currentSong->ArtistIds();
                m_song.edited.artistIds = m_song.original.artistIds;
            }
            if (m_song.original.lastSubsongIndex != currentSong->GetLastSubsongIndex() || m_song.original.subsongs[0].isInvalid != currentSong->IsInvalid())
            {
                currentSong->CopySubsongsTo(&m_song.original);
                currentSong->CopySubsongsTo(&m_song.edited);
            }
            else for (uint16_t i = 0, e = currentSong->GetLastSubsongIndex(); i <= e; i++)
            {
                if (m_song.original.subsongs[i].isDiscarded != currentSong->IsSubsongDiscarded(i))
                {
                    m_song.original.subsongs[i].isDiscarded = currentSong->IsSubsongDiscarded(i);
                    m_song.edited.subsongs[i].isDiscarded = m_song.original.subsongs[i].isDiscarded;
                }
                if (m_song.original.subsongs[i].name != currentSong->GetSubsongName(i))
                {
                    m_song.original.subsongs[i].name = currentSong->GetSubsongName(i);
                    m_song.edited.subsongs[i].name = m_song.original.subsongs[i].name;
                }
                if (m_song.original.subsongs[i].rating != currentSong->GetSubsongRating(i))
                {
                    m_song.original.subsongs[i].rating = currentSong->GetSubsongRating(i);
                    m_song.edited.subsongs[i].rating = m_song.original.subsongs[i].rating;
                }
                if (m_song.original.subsongs[i].state != currentSong->GetSubsongState(i))
                {
                    m_song.original.subsongs[i].state = currentSong->GetSubsongState(i);
                    m_song.edited.subsongs[i].state = m_song.original.subsongs[i].state;
                }
            }
            if (m_song.original.sourceIds != currentSong->SourceIds())
            {
                m_song.original.sourceIds = currentSong->SourceIds();
                m_song.edited.sourceIds = m_song.original.sourceIds;
            }
            if (m_song.original.metadata != currentSong->Metadatas())
            {
                m_song.original.metadata = currentSong->Metadatas();
                m_song.edited.metadata = m_song.original.metadata;
            }
            if (m_song.original.tags != currentSong->GetTags())
            {
                m_song.original.tags = currentSong->GetTags();
                m_song.edited.tags = m_song.original.tags;
            }
            m_selectedSubsong = Min(m_selectedSubsong, currentSong->GetLastSubsongIndex());
        }
        else
        {
            m_musicId = {};
            if (m_song.original.id != SongID::Invalid)
            {
                m_song.original = {};
                m_song.edited = {};
            }
            m_selectedSubsong = 0;
        }

        // edit the song
        ImGui::BeginDisabled(currentSong == nullptr);
        if (ImGui::BeginTable("song", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody, ImVec2(ImGui::CalcTextSize("000000000000 id000000c | 0000.0000 | 0000/00/00 | Release Year 000000").x, 0.0f)))
        {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Info:");
            ImGui::TableNextColumn();
            if (currentSong)
            {
                auto added = std::chrono::year_month_day(std::chrono::sys_days(std::chrono::days(currentSong->GetDatabaseDay() + Core::kReferenceDate)));
                auto sizeFormat = GetSizeFormat(currentSong->GetFileSize());
                ImGui::Text("id%06X%c | %.2f%s | %04d/%02u/%02u | Release Year", static_cast<uint32_t>(m_musicId.subsongId.songId)
                    , m_musicId.databaseId == DatabaseID::kPlaylist ? 'P' : 'L'
                    , currentSong->GetFileSize() / sizeFormat.second, sizeFormat.first
                    , int32_t(added.year()), uint32_t(added.month()), uint32_t(added.day()));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                int32_t year = m_song.edited.releaseYear;
                ImGui::DragInt("##Year", &year, 1.0f, 0, 65535, "%04d", ImGuiSliderFlags_AlwaysClamp);
                m_song.edited.releaseYear = uint16_t(year);
            }
            ImGui::Spacing();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Title:");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##Title", &m_song.edited.name.String());
            ImGui::Spacing();

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Subsong:", false))
                m_isSubsongOpened = !m_isSubsongOpened;
            ImGui::TableNextColumn();
            EditSubsongs();
            ImGui::Spacing();


            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Artist:");
            ImGui::TableNextColumn();
            EditArtists();
            ImGui::Spacing();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Tags:");
            ImGui::TableNextColumn();
            EditTags();
            ImGui::Spacing();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Player:");
            ImGui::TableNextColumn();
            EditPlayer();
            ImGui::Spacing();

            ImGui::TableNextColumn();
            if (ImGui::Selectable("Settings:", false))
                m_isMetadataOpened = !m_isMetadataOpened;
            ImGui::TableNextColumn();
            EditMetadata();
            ImGui::Spacing();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Source:");
            ImGui::TableNextColumn();
            EditSources();
            ImGui::Spacing();

            SaveChanges(currentSong);
            ImGui::Spacing();

            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        ImGui::EndBusy(m_busyTime, 48.0f, 16.0f, 64, 0.7f, m_busyColor);
        ImGui::EndDisabled();
    }

    void SongEditor::OnEndUpdate()
    {
        if (m_isBusy)
            m_busyTime += ImGui::GetIO().DeltaTime;
        else
            m_busyTime = 0.0f;
    }

    void SongEditor::EditSubsongs()
    {
        char indexFormat[] = "#%01u";
        for (auto count = m_song.edited.lastSubsongIndex + 1; count >= 10; count /= 10)
            indexFormat[3]++;

        ImGui::PushID("EditSubsongs");
        uint32_t numValidSubsongs = 0;
        for (uint16_t i = 0, e = m_song.edited.lastSubsongIndex; i <= e; i++)
        {
            if (!m_song.edited.subsongs[i].isDiscarded)
                numValidSubsongs++;
        }
        if (!m_isSubsongOpened && m_song.edited.lastSubsongIndex > 0)
        {
            auto& subsong = m_song.edited.subsongs[m_selectedSubsong];
            ImGui::PushID(m_selectedSubsong | 0x80000000);
            char txt[16];
            sprintf(txt, indexFormat, m_selectedSubsong + 1);
            ImGui::SetNextItemWidth(ImGui::CalcTextSize(txt).x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::BeginCombo("##ComboSubsongs", txt, ImGuiComboFlags_PopupAlignLeft))
            {
                for (uint16_t i = 0, e = m_song.edited.lastSubsongIndex; i <= e; i++)
                {
                    ImGui::PushID(i);
                    auto isSelected = m_selectedSubsong == i;
                    if (ImGui::Selectable("##SelectSubsong", isSelected, ImGuiSelectableFlags_AllowOverlap))
                        m_selectedSubsong = i;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::SameLine(0.0f, 0.0f);
                    sprintf(txt, indexFormat, i + 1);
                    ImGui::TextUnformatted(txt);
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            EditSubsong(subsong, numValidSubsongs);
        }
        else for (uint16_t i = 0, e = m_song.edited.lastSubsongIndex; i <= e; i++)
        {
            auto& subsong = m_song.edited.subsongs[i];
            ImGui::PushID(i);
            ImGui::BeginDisabled(true);
            char txt[16];
            sprintf(txt, indexFormat, i + 1);
            ImGui::Button(txt);
            ImGui::EndDisabled();
            EditSubsong(subsong, numValidSubsongs);
        }
        ImGui::PopID();
    }

    void SongEditor::EditSubsong(SubsongData<Blob::kIsDynamic>& subsong, uint32_t numValidSubsongs)
    {
        const auto buttonSize = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();
        ImGui::SameLine();
        bool isSubsongValid = !subsong.isDiscarded;
        ImGui::BeginDisabled(isSubsongValid && numValidSubsongs == 1);
        if (ImGui::Checkbox("##Valid", &isSubsongValid) && !isSubsongValid)
            subsong.name.String().clear();
        ImGui::EndDisabled();
        subsong.isDiscarded = !isSubsongValid;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (buttonSize + style.ItemInnerSpacing.x) * 3));
        ImGui::InputText("##Name", &subsong.name.String());
        ImGui::SameLine();
        static const char* const subsongStateLabels[] = { "U", "S", "F", "L" };
        static const char* const subsongStates[] = { "Undefined", "Stop", "Fade Out", "Loop Once" };
        if (ImGui::Button(subsongStateLabels[uint32_t(subsong.state)], ImVec2(buttonSize, 0.0f)))
            ImGui::OpenPopup("StatePopup");
        if (ImGui::BeginPopup("StatePopup"))
        {
            for (uint32_t i = 0; i < kNumSubsongStates; i++)
            {
                auto state = SubsongState(i);
                if (ImGui::Selectable(subsongStates[i], subsong.state == state))
                    subsong.state = state;
            }
            ImGui::EndPopup();
        }
        else if (ImGui::IsItemHovered())
            ImGui::Tooltip(subsongStates[uint32_t(subsong.state)]);
        ImGui::SameLine();
        int32_t subsongRating = subsong.rating;
        if (subsongRating == 0)
            ImGui::ProgressBar(0.0f, ImVec2(buttonSize * 2 + style.ItemInnerSpacing.x, 0.0f), "N/A");
        else
            ImGui::ProgressBar((subsongRating - 1) * 0.01f, ImVec2(buttonSize * 2 + style.ItemInnerSpacing.x, 0.0f));
        if (ImGui::BeginPopupContextItem("Rating", ImGuiPopupFlags_MouseButtonLeft))
        {
            char label[8] = "N/A";
            if (subsongRating > 0)
                sprintf(label, "%u%%%%", subsongRating - 1);
            ImGui::SliderInt("##rating", &subsongRating, 0, 101, label, ImGuiSliderFlags_NoInput);
            subsong.rating = uint8_t(subsongRating);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    void SongEditor::EditArtists()
    {
        ImGui::PushID("EditArtists");
        const float buttonSize = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();

        ImGuiID artistPopupId = ImGui::GetID("ArtistPopup");
        int artistToRemove = -1;
        for (int i = 0, lastIndex = m_song.edited.artistIds.NumItems<int>() - 1; i <= lastIndex; i++)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(Max(1.0f, ImGui::CalcItemWidth() - (buttonSize + style.ItemInnerSpacing.x) * 3));
            auto artist = m_musicId.GetArtist(m_song.edited.artistIds[i]);
            ImGui::InputText("##artist", const_cast<char*>(artist->GetHandle()), strlen(artist->GetHandle()) + 1, ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(i == 0);
            if (ImGui::Button("^", ImVec2(buttonSize, 0.0f)))
                std::swap(m_song.edited.artistIds[i - 1], m_song.edited.artistIds[i]);
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Move up");
            ImGui::EndDisabled();
            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            if (i == lastIndex)
            {
                ImGui::BeginDisabled(m_song.edited.artistIds.NumItems() == Core::GetDatabase(m_musicId.databaseId).NumArtists());
                if (ImGui::Button("+", ImVec2(buttonSize, 0.0f)))
                {
                    ImGui::OpenPopup(artistPopupId);
                    if (m_artistFilter == nullptr)
                        m_artistFilter = new ImGuiTextFilter();
                }
                ImGui::EndDisabled();
            }
            else if (ImGui::Button("v", ImVec2(buttonSize, 0.0f)))
                std::swap(m_song.edited.artistIds[i + 1], m_song.edited.artistIds[i]);
            if (ImGui::IsItemHovered())
                ImGui::Tooltip(i == lastIndex ? "Add another artist" : "Move down");
            ImGui::SameLine(0, style.ItemInnerSpacing.x);
            if (ImGui::Button("X", ImVec2(buttonSize, 0.0f)))
                artistToRemove = i;
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Remove artist");
            ImGui::PopID();
        }
        if (artistToRemove >= 0)
            m_song.edited.artistIds.RemoveAt(artistToRemove);
        if (ImGui::BeginPopup("ArtistPopup"))
        {
            // Draw filter
            m_artistFilter->Draw("##filter", m_artistMaxWidth);

            // Build and sort the artist list
            auto& db = Core::GetDatabase(m_musicId.databaseId);
            Array<Artist*> artists(size_t(0), db.NumArtists());
            float artistMaxWidth = 64.0f;
            for (Artist* artist : db.Artists())
            {
                if (m_artistFilter->PassFilter(artist->GetHandle()) && !m_song.edited.artistIds.Find(artist->GetId()))
                {
                    // sad but I have to build the with myself as imgui doesn't want to autfit properly in the child window...
                    artistMaxWidth = Max(artistMaxWidth, ImGui::CalcTextSize(artist->GetHandle()).x);
                    artists.Add(artist);
                }
            }
            artistMaxWidth += ImGui::GetIO().FontGlobalScale * 16.0f;
            m_artistMaxWidth = artistMaxWidth;
            std::sort(artists.begin(), artists.end(), [this](auto l, auto r)
            {
                return _stricmp(l->GetHandle(), r->GetHandle()) < 0;
            });

            // Select and add the artist
            ImGui::BeginChild("ArtistList", ImVec2(artistMaxWidth, ImGui::GetTextLineHeightWithSpacing() * Min(32u, artists.NumItems())));
            ImGuiListClipper clipper;
            clipper.Begin(artists.NumItems<int32_t>());
            while (clipper.Step())
            {
                for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                {
                    auto* artist = artists[rowIdx];
                    ImGui::PushID(static_cast<int>(artist->GetId()));
                    if (ImGui::Selectable(artist->GetHandle()))
                    {
                        m_song.edited.artistIds.Add(artist->id);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
            }

            ImGui::EndChild();

            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    void SongEditor::EditTags()
    {
        if (ImGui::BeginTable("Tags", 4, ImGuiTableFlags_SizingStretchSame))
        {
            for (uint32_t i = 0; i < Tag::kNumTags; i++)
            {
                ImGui::TableNextColumn();
                bool isChecked = m_song.edited.tags.IsEnabled(1ull << i);
                if (ImGui::Checkbox(Tag::Name(i), &isChecked))
                    m_song.edited.tags.Enable(1ull << i, isChecked);
            }
            for (uint32_t i = Tag::kNumTags; i < ((Tag::kNumTags + 3) & ~3); i++)
                ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, 0xff0000ff);
            ImGui::PushStyleColor(ImGuiCol_CheckMark, 0xff0000ff);
            bool isChecked = m_song.edited.subsongs[0].isInvalid;
            if (ImGui::Checkbox("Invalid", &isChecked))
                m_song.edited.subsongs[0].isInvalid = isChecked;
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Tagged as invalid will clear all subsongs data");
            ImGui::EndTable();
        }
    }

    void SongEditor::EditPlayer()
    {
        if (ImGui::BeginTable("Playback", 2, ImGuiTableFlags_SizingStretchProp))
        {
            auto& replays = Core::GetReplays();

            ImGui::TableNextColumn();
            ImGui::Text("Replay");
            ImGui::TableNextColumn();
            ImGui::Text("Extension");
            ImGui::TableNextColumn();
            if (ImGui::Button("Scan") && m_playables[0] == eReplay::Unknown)
            {
                m_isBusy = true;
                Core::AddJob([this, &replays, musicId = m_musicId]()
                {
                    SmartPtr<core::io::Stream> stream = musicId.GetStream();
                    Core::FromJob([this, musicId, playables = stream.IsValid() ? replays.Enumerate(stream) : Replayables()]()
                    {
                        m_isBusy = false;
                        if (m_musicId == musicId)
                            m_playables = playables;
                    });
                });
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Scan for all matching replays\nThey will be displayed first");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(-FLT_MIN);

            if (ImGui::BeginCombo("##ReplayCB", replays.GetName(m_song.edited.type.replay), ImGuiComboFlags_None))
            {
                uint32_t numReplayables = 0;
                for (uint32_t i = 0; i < uint32_t(eReplay::Count) && m_playables[i] != eReplay::Unknown; i++, numReplayables++)
                {
                    ImGui::PushID(int(m_playables[i]));
                    const bool isItemSelected = m_playables[i] == m_song.edited.type.replay;
                    if (ImGui::Selectable(replays.GetName(m_playables[i]), isItemSelected))
                        m_song.edited.type.replay = m_playables[i];
                    ImGui::PopID();
                }

                if (m_playables[0] != eReplay::Unknown)
                    ImGui::Separator();

                for (uint32_t i = 0; i < uint32_t(eReplay::Count); ++i)
                {
                    auto replaysEnd = m_playables.replays + numReplayables;
                    bool isReplayable = std::find(m_playables.replays, replaysEnd, eReplay(i)) != replaysEnd;
                    if (!isReplayable)
                    {
                        ImGui::PushID(i);
                        const bool isItemSelected = i == uint32_t(m_song.edited.type.replay);
                        if (ImGui::Selectable(replays.GetName(eReplay(i)), isItemSelected))
                            m_song.edited.type.replay = eReplay(i);
                        ImGui::PopID();
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(m_song.edited.subsongs[0].isArchive);
            auto extIndex = MediaType::mapSortedExtensions[int32_t(m_song.edited.type.ext)];
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
            ImGui::Combo("##Extension", &extIndex, MediaType::sortedExtensionNames, int32_t(eExtension::Count));
            m_song.edited.type.ext = MediaType::sortedExtensions[extIndex];
            ImGui::EndDisabled();

            ImGui::EndTable();
        }
    }

    void SongEditor::EditMetadata()
    {
        if (!m_isMetadataOpened)
        {
            ImGui::TextUnformatted("Unfold to edit the settings");
            return;
        }

        ReplayMetadataContext context(m_song.edited.metadata.Container(), m_song.edited.lastSubsongIndex);
        if (m_songEndEditor && m_songEndEditor->Update(context))
        {
            delete m_songEndEditor;
            m_songEndEditor = nullptr;
        }

        Core::GetReplays().EditMetadata(m_song.edited.type.replay, context);

        if (context.isSongEndEditorEnabled)
            m_songEndEditor = SongEndEditor::Create(context, m_musicId);
    }

    void SongEditor::EditSources()
    {
        if (m_song.edited.sourceIds.IsEmpty())
        {
            ImGui::NewLine();
            return;
        }

        ImGui::PushID("EditSources");
        const float button_size = ImGui::GetFrameHeight();
        auto& style = ImGui::GetStyle();

        char txt[256];
        {
            auto source = m_song.edited.sourceIds[0];
            ImGui::PushID(source.value);
            sprintf_s(txt, "ID: %06X %s", source.internalId, SourceID::sourceNames[source.sourceId]);
            ImGui::SetNextItemWidth(-FLT_MIN);
            auto width = ImGui::CalcItemWidth();
            if (m_song.edited.sourceIds.NumItems() > 1)
                ImGui::SetNextItemWidth(Max(1.0f, width - (button_size + style.ItemInnerSpacing.x)));
            ImGui::InputText("##source", txt, sizeof(txt), ImGuiInputTextFlags_ReadOnly);
            if (m_song.edited.sourceIds.NumItems() > 1)
            {
                width -= style.ItemInnerSpacing.x * 0.5f;
                ImGui::SameLine();
                if (ImGui::BeginCombo("##sources", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft))
                {
                    uint16_t sourceToRestore = 0;
                    for (uint16_t i = 1, e = m_song.edited.sourceIds.NumItems<uint16_t>(); i < e; i++)
                    {
                        source = m_song.edited.sourceIds[i];
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::SetNextItemWidth(Max(1.0f, width - (button_size + style.ItemInnerSpacing.x) * 3));
                        sprintf_s(txt, "ID: %06X %s", source.internalId, SourceID::sourceNames[source.sourceId]);
                        ImGui::InputText("##source", txt, sizeof(txt), ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine(0, style.ItemInnerSpacing.x);
                        ImGui::BeginDisabled(i == 1);
                        if (ImGui::Button("^", ImVec2(button_size, 0.0f)))
                            std::swap(m_song.edited.sourceIds[i - 1], m_song.edited.sourceIds[i]);
                        if (ImGui::IsItemHovered())
                            ImGui::Tooltip("Move up");
                        ImGui::EndDisabled();
                        ImGui::SameLine(0, style.ItemInnerSpacing.x);
                        ImGui::BeginDisabled(i == (e - 1));
                        if (ImGui::Button("v", ImVec2(button_size, 0.0f)))
                            std::swap(m_song.edited.sourceIds[i + 1], m_song.edited.sourceIds[i]);
                        if (ImGui::IsItemHovered())
                            ImGui::Tooltip("Move down");
                        ImGui::EndDisabled();
                        ImGui::SameLine(0, style.ItemInnerSpacing.x);
                        ImGui::BeginDisabled(e <= 1);
                        if (ImGui::Button("X", ImVec2(button_size, 0.0f)))
                            sourceToRestore = i;
                        if (ImGui::IsItemHovered())
                            ImGui::Tooltip("Invalidate source\n");
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                    if (sourceToRestore)
                        m_song.edited.sourceIds.RemoveAt(sourceToRestore);
                    ImGui::EndCombo();
                }
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    void SongEditor::SaveChanges(Song* currentSong)
    {
        auto isEnabled = currentSong != nullptr;
        auto isTitleUpdated = isEnabled && (currentSong->GetName() != m_song.edited.name);
        auto areSubsongsUpdated = false;
        if (isEnabled)
        {
            areSubsongsUpdated = currentSong->IsInvalid() != m_song.edited.subsongs[0].isInvalid;
            for (uint16_t i = 0; !areSubsongsUpdated && i <= currentSong->GetLastSubsongIndex(); i++)
            {
                areSubsongsUpdated |= currentSong->GetSubsongName(i) != m_song.edited.subsongs[i].name;
                areSubsongsUpdated |= currentSong->IsSubsongDiscarded(i) != m_song.edited.subsongs[i].isDiscarded;
                areSubsongsUpdated |= currentSong->GetSubsongRating(i) != m_song.edited.subsongs[i].rating;
                areSubsongsUpdated |= currentSong->GetSubsongState(i) != m_song.edited.subsongs[i].state;
            }
        }
        auto areArtistsUpdated = isEnabled && (currentSong->ArtistIds() != m_song.edited.artistIds);
        auto areTagsUpdated = isEnabled && (currentSong->GetTags() != m_song.edited.tags);
        auto areMetadataUpdated = isEnabled && (currentSong->Metadatas() != m_song.edited.metadata);
        auto areSourcesUpdated = isEnabled && (currentSong->SourceIds() != m_song.edited.sourceIds);
        auto isExtensionUpdated = isEnabled && (currentSong->GetType().ext != m_song.edited.type.ext);
        auto isReplayUpdated = isEnabled && (currentSong->GetType().replay != m_song.edited.type.replay);
        auto isTypeUpdated = isEnabled && (isExtensionUpdated || isReplayUpdated);
        auto isYearUpdated = isEnabled && (currentSong->GetReleaseYear() != m_song.edited.releaseYear);
        auto isUpdated = isTitleUpdated || areSubsongsUpdated || areArtistsUpdated || areTagsUpdated || areMetadataUpdated || areSourcesUpdated || isTypeUpdated || isYearUpdated;

        ImGui::BeginDisabled(!isUpdated);
        if (ImGui::Button("Save Changes", ImVec2(-FLT_MIN, 0)))
        {
            auto currentSongEdit = currentSong->Edit();

            if (isTitleUpdated || areArtistsUpdated || isExtensionUpdated)
            {
                if (m_musicId.databaseId == DatabaseID::kLibrary)
                {
                    auto& db = Core::GetLibraryDatabase();
                    auto oldFileName = db.GetFullpath(currentSong);
                    if (io::File::IsExisting(oldFileName.c_str()))
                    {
                        m_song.edited.AddRef(); // keep ownership
                        SmartPtr<Song> song = Song::Create(&m_song.edited);
                        db.Move(oldFileName, song, "SongEditor");
                    }
                }
            }
            if (areArtistsUpdated)
            {
                for (auto artistId : m_song.edited.artistIds)
                {
                    m_musicId.GetArtist(artistId)->Edit()->numSongs++;
                }
                for (auto artistId : currentSongEdit->artistIds)
                {
                    auto artist = m_musicId.GetArtist(artistId);
                    if (--artist->Edit()->numSongs == 0)
                        Core::GetDatabase(m_musicId.databaseId).RemoveArtist(artistId);
                }
                Core::GetDatabase(m_musicId.databaseId).Raise(Database::Flag::kSaveArtists);
            }
            if (areSourcesUpdated)
            {
                // start at 1 because we are not allowing to update the master source
                for (uint16_t i = 1, e = currentSongEdit->sourceIds.NumItems<uint16_t>(); i < e; i++)
                {
                    auto sourceId = currentSongEdit->sourceIds[i];
                    if (m_song.edited.sourceIds.Find(sourceId) == nullptr)
                    {
                        auto newSong = new SongSheet();
                        newSong->tags = m_song.edited.tags;
                        newSong->releaseYear = currentSongEdit->releaseYear;
                        newSong->type = currentSongEdit->type;
                        newSong->name = m_song.edited.name;
                        newSong->artistIds = m_song.edited.artistIds;
                        newSong->sourceIds.Add(sourceId);
                        newSong->metadata = m_song.edited.metadata;
                        Core::GetLibraryDatabase().AddSong(newSong);
                        for (auto artistId : newSong->artistIds)
                        {
                            Core::GetLibraryDatabase()[artistId]->Edit()->numSongs++;
                        }

                        Core::GetLibrary().m_sources[sourceId.sourceId]->InvalidateSong(sourceId, newSong->id);
                    }
                }
            }

            currentSongEdit->releaseYear = m_song.edited.releaseYear;
            currentSongEdit->type = m_song.edited.type;
            currentSongEdit->name = m_song.edited.name;
            currentSongEdit->artistIds = m_song.edited.artistIds;
            currentSongEdit->tags = m_song.edited.tags;
            currentSongEdit->metadata = m_song.edited.metadata;
            currentSongEdit->sourceIds = m_song.edited.sourceIds;

            if (areSubsongsUpdated)
            {
                if (m_song.edited.subsongs[0].isInvalid != m_song.original.subsongs[0].isInvalid)
                {
                    if (m_song.edited.subsongs[0].isInvalid)
                    {
                        m_song.edited.subsongs.Resize(1);
                        m_song.edited.subsongs[0].name = {};
                        m_song.edited.subsongs[0].isDiscarded = false;
                        m_song.edited.subsongs[0].rating = 0;
                        m_song.edited.subsongs[0].state = SubsongState::Undefined;
                        m_song.edited.subsongs[0].isInvalid = true;
                        m_song.edited.lastSubsongIndex = 0;

                        currentSongEdit->subsongs[0].durationCs = 0;
                    }

                    currentSongEdit->subsongs[0].isInvalid = m_song.edited.subsongs[0].isInvalid;
                }
                for (uint16_t i = 0; i <= m_song.edited.lastSubsongIndex; i++)
                {
                    if (m_song.edited.subsongs[i].isDiscarded && !currentSongEdit->subsongs[i].isDiscarded)
                    {
                        SubsongID subsongIdToRemove(currentSongEdit->id, i);
                        Core::Discard(MusicID(subsongIdToRemove, m_musicId.databaseId));
                    }
                    currentSongEdit->subsongs[i].isDiscarded = m_song.edited.subsongs[i].isDiscarded;
                    currentSongEdit->subsongs[i].name = m_song.edited.subsongs[i].name;
                    currentSongEdit->subsongs[i].rating = m_song.edited.subsongs[i].rating;
                    currentSongEdit->subsongs[i].state = m_song.edited.subsongs[i].state;
                }
                for (uint16_t i = m_song.edited.lastSubsongIndex + 1; i <= currentSongEdit->lastSubsongIndex; i++)
                {
                    SubsongID subsongIdToRemove(currentSongEdit->id, i);
                    Core::Discard(MusicID(subsongIdToRemove, m_musicId.databaseId));
                }
                currentSongEdit->lastSubsongIndex = m_song.edited.lastSubsongIndex;
            }

            if (areMetadataUpdated)
                Core::GetDeck().ChangedSettings();

            if (isTypeUpdated)
            {
                for (uint16_t i = 1; i <= currentSongEdit->lastSubsongIndex; i++)
                {
                    SubsongID subsongIdToRemove(currentSongEdit->id, i);

                    // todo: remove System::Discard to do only Playlist::Discard
                    Core::Discard(MusicID(subsongIdToRemove, m_musicId.databaseId));
                }
                currentSongEdit->lastSubsongIndex = 0;
                currentSongEdit->subsongs[0].Clear();
                CommandBuffer(currentSongEdit->metadata.Container()).Remove(uint16_t(m_song.original.type.replay));
            }

            m_musicId.MarkForSave();
        }
        ImGui::EndDisabled();
    }
}
// namespace rePlayer