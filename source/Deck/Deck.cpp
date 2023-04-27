#include "Deck.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>

#include <Database/SongEditor.h>
#include <Deck/Player.h>
#include <Library/Library.h>
#include <Playlist/Playlist.h>
#include <Replayer/About.h>
#include <Replayer/Core.h>
#include <Replayer/Settings.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cmath>
#include <ctime>

namespace rePlayer
{
    Deck::Deck()
        : Window("System", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking, true)
    {
        Enable(true);
        m_volume = Player::GetVolume();
        Player::SetVolume(m_volume);
    }

    Deck::~Deck()
    {}

    void Deck::PlaySolo(MusicID musicId)
    {
        if (musicId.subsongId.IsValid())
            PlaySolo(Core::GetPlaylist().LoadSong(musicId));
    }

    void Deck::PlaySolo(const SmartPtr<Player>& player)
    {
        if (player.IsValid())
        {
            if (m_mode != Mode::Solo)
            {
                //push current player stack
                if (m_currentPlayer.IsValid())
                    m_currentPlayer->Pause();
                m_shelvedPlayer = std::move(m_currentPlayer);
                m_mode = Mode::Solo;
            }
            m_currentPlayer = player;
            Play(Tracking::IsDisabled);
        }
    }

    void Deck::Play(const SmartPtr<Player>& player1, const SmartPtr<Player>& player2)
    {
        if (player1.IsValid())
        {
            m_shelvedPlayer.Reset();
            m_currentPlayer = player1;
            m_nextPlayer = player2;
            m_mode = Mode::Playlist;
            Play();
        }
    }

    void Deck::PlayPause()
    {
        auto player = m_currentPlayer;
        if (player.IsValid())
        {
            if (!player->IsPlaying())
                Play();
            else
                player->Pause();
        }
    }

    void Deck::Next()
    {
        PlayNextSong();
    }

    void Deck::Previous()
    {
        PlayPreviousSong();
    }

    void Deck::Stop()
    {
        auto player = m_currentPlayer;
        if (player.IsValid() && !player->IsStopped())
            player->Stop();
        player = m_nextPlayer;
        if (player.IsValid() && !player->IsStopped())
            player->Stop();
    }

    void Deck::IncreaseVolume()
    {
        m_volume = Clamp(m_volume + 256, 0, 65535);
        Player::SetVolume(m_volume);
    }

    void Deck::DecreaseVolume()
    {
        m_volume = Clamp(m_volume - 256, 0, 65535);
        Player::SetVolume(m_volume);
    }

    void Deck::OnNewPlaylist()
    {
        m_shelvedPlayer.Reset();
        m_nextPlayer.Reset();
        if (m_mode == Mode::Playlist)
        {
            m_currentPlayer.Reset();
            m_mode = Mode::Solo;
        }
    }

    void Deck::DisplayProgressBarInTable(float backgroundRatio)
    {
        if (Player* player = m_currentPlayer)
        {
            auto table = ImGui::GetCurrentTable();
            auto tableInstanceData = ImGui::TableGetInstanceData(table, table->InstanceCurrent);

            auto playbackTime = player->GetPlaybackTimeInMs();
            auto duration = Max(1u, player->GetSubsong().durationCs * 10, playbackTime);
            float ratio = playbackTime / static_cast<float>(duration);
            auto x = table->BorderX1 + table->ColumnsGivenWidth * ratio;

            auto color1 = ImVec4(std::lerp(0.30f, 1.0f, backgroundRatio), std::lerp(0.50f, 1.0f, backgroundRatio), std::lerp(0.30f, 1.0f, backgroundRatio), 1.0f);
            auto color2 = ImVec4(std::lerp(0.15f, 1.0f, backgroundRatio), std::lerp(0.25f, 1.0f, backgroundRatio), std::lerp(0.15f, 1.0f, backgroundRatio), 1.0f);

            //TablePushBackgroundChannel/TablePopBackgroundChannel is the hack I need to make it work as intended (progress bar drawn in the background of the table)
            ImGui::TablePushBackgroundChannel();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(ImVec2(table->BorderX1, table->RowPosY1)
                , ImVec2(x, table->RowPosY2 + tableInstanceData->LastFirstRowHeight + 1)
                , ImGui::GetColorU32(color1));
            draw_list->AddRectFilled(ImVec2(x, table->RowPosY1)
                , ImVec2(table->BorderX2, table->RowPosY2 + tableInstanceData->LastFirstRowHeight + 1)
                , ImGui::GetColorU32(color2));
            ImGui::TablePopBackgroundChannel();

            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32_DISABLE);
        }
        else
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.15f, 0.25f, 0.15f, 1.0f)));
    }

    MusicID Deck::GetCurrentPlayerId() const
    {
        MusicID musicId;
        if (Player* player = m_currentPlayer)
            musicId = m_currentPlayer->GetId();
        return musicId;
    }

    std::string Deck::GetMetadata(const MusicID musicId) const
    {
        if (m_currentPlayer.IsValid() && m_currentPlayer->GetId().subsongId == musicId.subsongId && m_currentPlayer->GetId().databaseId == musicId.databaseId)
            return m_currentPlayer->GetMetadata();
        if (m_shelvedPlayer.IsValid() && m_shelvedPlayer->GetId().subsongId == musicId.subsongId && m_shelvedPlayer->GetId().databaseId == musicId.databaseId)
            return m_shelvedPlayer->GetMetadata();
        if (m_nextPlayer.IsValid() && m_nextPlayer->GetId().subsongId == musicId.subsongId && m_nextPlayer->GetId().databaseId == musicId.databaseId)
            return m_nextPlayer->GetMetadata();
        return std::string();
    }

    Status Deck::UpdateFrame()
    {
        auto windowStates = m_windowStates;
        Window::Update(windowStates);

        return m_isOpened ? Status::kOk : Status::kFail;
    }

    void Deck::DisplaySettings()
    {
        if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_None))
        {
            ImGui::Checkbox("Focus current playing song in Playlist", &m_isTrackingSongInPlaylist);
            ImGui::Checkbox("Focus current playing song in Database", &m_isTrackingSongInDatabase);
            uint32_t saveFrequency = (m_saveFrequency + 59) / 60;
            uint32_t saveMin = 0;
            uint32_t saveMax = 24 * 60;
            if (ImGui::SliderScalar("AutoSave", ImGuiDataType_U32, &saveFrequency, &saveMin, &saveMax, "Every %u minutes", ImGuiSliderFlags_AlwaysClamp))
                m_saveFrequency = saveFrequency * 60;
            ImGui::SliderFloat("Transparency", &m_blendingFactor, 0.25f, 1.0f, "", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
            Log::DisplaySettings();
        }
    }

    void Deck::ChangedSettings()
    {
        if (m_currentPlayer.IsValid())
            m_currentPlayer->ApplySettings();
        if (m_shelvedPlayer.IsValid())
            m_shelvedPlayer->ApplySettings();
        if (m_nextPlayer.IsValid())
            m_nextPlayer->ApplySettings();
    }

    void Deck::OnSystray(int32_t x, int32_t y, SystrayState state)
    {
        if (m_isSystrayMenuEnabled && state == SystrayState::kMouseButtonRight)
            return;
        if (state == SystrayState::kMouseButtonLeft)
        {
            m_windowStates.isEnabled = !m_windowStates.isEnabled;
            Enable(m_windowStates.isEnabled);
            m_hSystrayWnd = nullptr;
            return;
        }
        else if (state == SystrayState::kMouseButtonMiddle)
        {
            InvertWindowStates();
            return;
        }
        m_systrayPos[0] = static_cast<float>(x);
        m_systrayPos[1] = static_cast<float>(y);
        m_isSystrayEnabled = state == SystrayState::kMouseButtonRight;
        m_isSystrayMenuEnabled = state == SystrayState::kMouseButtonRight;
        if (state == SystrayState::kTooltipOpen)
            m_isSystrayBalloonEnabled = true;
        else if (state == SystrayState::kTooltipClose)
            m_isSystrayBalloonEnabled = false;
        m_hSystrayWnd = nullptr;
    }

    void Deck::OnBeginUpdate()
    {
        m_windowStates.isEnabled = IsEnabled();

        // update the playback
        if (m_currentPlayer.IsValid())
        {
            // we do here some validation because the loaded song can have been updated/deleted...
            bool hasChanged = false;
            bool isPlaying = m_currentPlayer->IsPlaying();
            bool isStopped = m_currentPlayer->IsStopped();

            // 1 - check for the playlist file in shelved mode (m_mode should be Mode::Solo)
            if (m_shelvedPlayer.IsValid() && (m_shelvedPlayer->GetSubsong().isDiscarded || m_shelvedPlayer->GetMediaType() != m_shelvedPlayer->GetSong()->type))
            {
                m_shelvedPlayer = Core::GetPlaylist().LoadCurrentSong();
                m_nextPlayer = Core::GetPlaylist().LoadNextSong(false);
            }

            // 2 - check solo vs playlist
            if (m_mode == Mode::Solo)
            {
                if (m_currentPlayer->GetMediaType() != m_currentPlayer->GetSong()->type)
                {
                    m_currentPlayer = Core::GetPlaylist().LoadSong(m_currentPlayer->GetId());
                    hasChanged = true;
                }
                if (m_currentPlayer.IsInvalid() || m_currentPlayer->GetSubsong().isDiscarded)
                {
                    if (m_shelvedPlayer.IsValid())
                    {
                        m_mode = Mode::Playlist;
                        m_currentPlayer = std::move(m_shelvedPlayer);
                        if (m_nextPlayer.IsValid() && (m_nextPlayer->GetSubsong().isDiscarded || m_nextPlayer->GetMediaType() != m_nextPlayer->GetSong()->type))
                            m_nextPlayer = Core::GetPlaylist().LoadNextSong(false);
                        hasChanged = true;
                    }
                    else
                        m_currentPlayer.Reset();
                }
            }
            else if (m_currentPlayer->GetSubsong().isDiscarded || m_currentPlayer->GetMediaType() != m_currentPlayer->GetSong()->type)
            {
                m_currentPlayer = Core::GetPlaylist().LoadCurrentSong();
                m_nextPlayer = Core::GetPlaylist().LoadNextSong(false);
                hasChanged = true;
            }
            else if (m_nextPlayer.IsValid() && (m_nextPlayer->GetSubsong().isDiscarded || m_nextPlayer->GetMediaType() != m_nextPlayer->GetSong()->type))
                m_nextPlayer = Core::GetPlaylist().LoadNextSong(false);
            // we could validate here, but we might spam the servers if we move entries in the playlist
            //else
            //    ValidateNextSong();

            // 3 - update state
            if (m_currentPlayer.IsValid())
            {
                if (hasChanged)
                {
                    if (isPlaying)
                        Play();
                    else if (isStopped)
                        m_currentPlayer->Stop();
                    else
                        m_currentPlayer->Pause();
                }
                else if (auto endState = m_currentPlayer->IsEnding(75))
                {
                    if (endState == Player::kEnded)
                        PlayNextSong();
                    else if (isPlaying && m_shelvedPlayer.IsInvalid() && m_nextPlayer.IsValid())
                    {
                        // start the next song without stopping the current one, to have a seamless playback
                        ValidateNextSong();
                        m_nextPlayer->Play();
                    }
                }
            }
        }
    }

    std::string Deck::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
        if (m_isSystrayEnabled)
            ImGui::SetNextWindowFocus();
        return "rePlayer";
    }

    void Deck::OnDisplay()
    {
        // Hack begin
        // We want this to be the "main" window
        auto viewport = ImGui::GetWindowViewport();
        viewport->Flags &= ~ImGuiViewportFlags_NoTaskBarIcon;
        // Hack end

        Core::GetPlaylist().UpdateDragDropSource(1);

        SongSheet* song = nullptr;
        auto player = m_currentPlayer;
        auto isPlayerValid = player.IsValid();

        // VuMeter
        ImGui::BeginGroup();
        ImGui::BeginChild("VuMeter", ImVec2(24.0f, m_VuMeterHeight), false, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);//child window to prevent the hover (then tooltip from histogram)
        StereoSample vuValue = {};
        if (isPlayerValid)
            vuValue = player->GetVuMeter();
        ImGui::PlotHistogram("##VuMeter", &vuValue.left, 2, 0, nullptr, 0.0f, 1.0f, ImVec2(24.0f, m_VuMeterHeight));
        ImGui::EndChild();

        // Volume
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 8);
        if (ImGui::VSliderInt("##Volume", ImVec2(10, m_VuMeterHeight), reinterpret_cast<int*>(&m_volume), 0, 65535, "", ImGuiSliderFlags_NoInput))
            Player::SetVolume(m_volume);
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered())
        {
            ImGui::Tooltip("Volume at %d%%", m_volume * 100 / 65535);
            if (ImGui::GetIO().MouseWheel != 0.0f)
            {
                auto volume = static_cast<float>(m_volume);
                auto speed = ImGui::GetIO().MouseWheel;
                auto shift = ::GetAsyncKeyState(VK_LSHIFT) | ::GetAsyncKeyState(VK_RSHIFT);
                if (shift & 0x8000)
                    speed *= 10;
                volume += speed * 65535 / ImGui::GetItemRectSize().y;
                volume = Max(Min(65535.0f, volume), 0.0f);
                m_volume = static_cast<uint32_t>(volume);
                Player::SetVolume(m_volume);
            }
        }
        ImGui::EndGroup();

        // Infos
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (isPlayerValid)
        {
            const auto musicId = player->GetId();
            song = player->GetSong();
            const auto subsongId = musicId.subsongId;

            std::string title = musicId.GetTitle();
            if (DrawClippedText(title))
            {
                auto metadata = player->GetMetadata();
                if (metadata.size())
                    ImGui::Tooltip(metadata.c_str());
            }

            DrawClippedArtists(musicId);

            DisplayInfoAndOscilloscope(player);

            auto playbackTime = player->GetPlaybackTimeInMs();
            auto duration = Max(1u, song->subsongs[subsongId.index].durationCs * 10, playbackTime);

            char buf[32];
            if (m_mode == Mode::Solo)
            {
                if (m_seekPos >= 0)
                    sprintf(buf, "%d:%02d/%d:%02d", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60);
                else
                    sprintf(buf, "%d:%02d/%d:%02d", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60);
            }
            else
            {
                auto& playlist = Core::GetPlaylist();
                if (m_seekPos >= 0)
                    sprintf(buf, "%d:%02d/%d:%02d [%d/%u]", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
                else
                    sprintf(buf, "%d:%02d/%d:%02d [%d/%u]", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
            }
            ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 4);
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            auto col = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
            bool isSeekDisabled = player->IsStopped() || !player->IsSeekable();
            if (isSeekDisabled)
                col.w *= 0.2f;
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, col);
            ImGui::SetNextItemWidth(-1.0f);
            uint32_t minPlaybackTime = 0;
            uint32_t maxPlaybackTime = duration;
            ImGui::BeginDisabled(isSeekDisabled);
            if (ImGui::SliderScalar("###progress", ImGuiDataType_U32 , &playbackTime, &minPlaybackTime, &maxPlaybackTime, buf, ImGuiSliderFlags_AlwaysClamp))
            {
                m_seekPos = playbackTime;
            }
            else if (m_seekPos >= 0)
            {
                player->Seek(uint32_t(m_seekPos));
                m_seekPos = -1;
            }
            ImGui::EndDisabled();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
        else
        {
            DrawClippedText(" ");
            DrawClippedText(" ");
            DrawClippedText("              /\\\n       <-  rePlayer  ->\n              \\/");

            ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 4);
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            auto col = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
            col.w = 0.f;
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, col);
            ImGui::SetNextItemWidth(-1.0f);
            int32_t pos{ 0 };
            ImGui::BeginDisabled(true);
            ImGui::SliderInt("###progress", &pos, 0, 0, "", ImGuiSliderFlags_NoInput);
            ImGui::EndDisabled();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }

        if (ImGui::Button("Prev"))
        {
            PlayPreviousSong();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop") && isPlayerValid && !player->IsStopped())
        {
            player->Stop();
            if (m_nextPlayer.IsValid())
                m_nextPlayer->Stop();
        }
        ImGui::SameLine();
        if (!isPlayerValid)
        {
            ImGui::Button("Play");
        }
        else if (!player->IsPlaying())
        {
            if (ImGui::Button("Play"))
                Play();
        }
        else if (ImGui::Button("Pause"))
        {
            player->Pause();
            if (m_nextPlayer.IsValid())
                m_nextPlayer->Pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Next"))
        {
            PlayNextSong();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(m_isLooping ? ImGuiCol_ButtonActive : ImGuiCol_Button));
        if (ImGui::Button("Loop"))
        {
            m_isLooping = !m_isLooping;
            ValidateNextSong();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Menu"))
            ImGui::OpenPopup("Windows");
        if (ImGui::BeginPopup("Windows"))
        {
            bool isEnabled = Core::GetSettings().IsEnabled();
            if (ImGui::MenuItem("Settings", "", &isEnabled))
                Core::GetSettings().Enable(isEnabled);
            ImGui::Separator();
            isEnabled = Core::GetLibrary().IsEnabled();
            if (ImGui::MenuItem("Library", "", &isEnabled))
                Core::GetLibrary().Enable(isEnabled);
            isEnabled = Core::GetPlaylist().IsEnabled();
            if (ImGui::MenuItem("Playlist", "", &isEnabled))
                Core::GetPlaylist().Enable(isEnabled);
            isEnabled = Core::GetSongEditor().IsEnabled();
            if (ImGui::MenuItem("Song Editor", "", &isEnabled))
                Core::GetSongEditor().Enable(isEnabled);
            isEnabled = Log::Get().IsEnabled();
            if (ImGui::MenuItem("Log", "", &isEnabled))
                Log::Get().Enable(isEnabled);
            ImGui::Separator();
            isEnabled = Core::GetAbout().IsEnabled();
            if (ImGui::MenuItem("About", "", &isEnabled))
                Core::GetAbout().Enable(isEnabled);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                m_isOpened = false;
            ImGui::EndPopup();
        }
        ImGui::EndGroup();
        m_VuMeterHeight = ImGui::GetItemRectSize().y;
    }

    void Deck::OnEndUpdate()
    {
        UpdateSystray();

        // update/reset current song parameters
        auto currentSubsongID = m_currentPlayer.IsValid() ? m_currentPlayer->GetId().subsongId : SubsongID();
        if (m_currentSubsongId != currentSubsongID)
        {
            m_textTime = m_maxTextSize = 0.0f;
            m_currentSubsongId = currentSubsongID;
        }
        else
            m_textTime += ImGui::GetIO().DeltaTime;
    }

    void Deck::OnEndFrame()
    {
        auto saveTime = m_saveTime + ImGui::GetIO().DeltaTime;
        if (uint32_t(saveTime) > m_saveFrequency)
        {
            Core::GetLibrary().Save();
            Core::GetPlaylist().Save();

            saveTime = 0.f;
        }
        m_saveTime = saveTime;
    }

    void Deck::OnApplySettings()
    {
        m_windowStates = GetStates();

        // should happen on imgui settings load
        // so, load our current songs if any
        m_currentPlayer = Core::GetPlaylist().LoadCurrentSong();
        if (m_currentPlayer.IsValid())
        {
            m_mode = Mode::Playlist;
            m_nextPlayer = Core::GetPlaylist().LoadNextSong(false);
        }
        Player::SetVolume(m_volume);
    }

    void Deck::PlayPreviousSong()
    {
        if (m_currentPlayer.IsValid() && !m_currentPlayer->IsStopped())
        {
            m_currentPlayer->Stop();
        }
        if (m_mode == Mode::Solo)
        {
            if (m_shelvedPlayer.IsValid())
            {
                std::swap(m_currentPlayer, m_shelvedPlayer);
                m_mode = Mode::Playlist;
                Play();
                m_shelvedPlayer.Reset();
            }
        }
        else
        {
            Core::GetPlaylist().LoadPreviousSong(m_currentPlayer, m_nextPlayer);
            if (m_currentPlayer.IsValid())
                Play();
        }
    }
    void Deck::PlayNextSong()
    {
        ValidateNextSong();

        if (m_currentPlayer.IsValid() && !m_currentPlayer->IsStopped())
        {
            m_currentPlayer->Stop();
        }
        if (m_mode == Mode::Solo)
        {
            if (m_shelvedPlayer.IsValid())
            {
                std::swap(m_currentPlayer, m_shelvedPlayer);
                m_mode = Mode::Playlist;
                Play();
                m_shelvedPlayer.Reset();
            }
        }
        else if (m_nextPlayer.IsValid())//m_mode == Mode::Playlist;
        {
            std::swap(m_currentPlayer, m_nextPlayer);
            Play();
            m_nextPlayer = Core::GetPlaylist().LoadNextSong(true);
        }
        else
        {
            m_currentPlayer.Reset();
            Core::GetPlaylist().Eject();
            m_mode = Mode::Solo;
        }
    }

    void Deck::ValidateNextSong()
    {
        if (m_mode == Mode::Solo && m_shelvedPlayer.IsValid())
            Core::GetPlaylist().ValidateNextSong(m_nextPlayer);
        else if (m_mode == Mode::Playlist && m_currentPlayer.IsValid())
            Core::GetPlaylist().ValidateNextSong(m_nextPlayer);
    }

    void Deck::Play(Tracking isTrackingEnabled)
    {
        if (m_currentPlayer.IsValid())
        {
            m_currentPlayer->Play();
            if (isTrackingEnabled == Tracking::IsEnabled)
            {
                if (m_isTrackingSongInPlaylist && m_mode == Mode::Playlist)
                    Core::GetPlaylist().FocusCurrentSong();
                if (m_isTrackingSongInDatabase && m_currentPlayer.IsValid())
                    m_currentPlayer->GetId().Track();
            }
        }
    }

    template <typename DrawCallback>
    bool Deck::DrawClippedText(const std::string& text, DrawCallback&& drawCallback)
    {
        bool isHoovered{ false };
        auto& style = ImGui::GetStyle();

        auto textSize = ImGui::CalcTextSize(text.c_str());
        //ImGui::InvisibleButton(label, ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2));
        //it's a hooverable ImGui::Dummy because we want to move the window while dragging this widget
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window->SkipItems)
        {
            auto size = ImGui::CalcItemSize(ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2), 0.0f, 0.0f);
            const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
            ImGui::ItemSize(size);
            ImGui::ItemAdd(bb, 0);
            isHoovered = ImGui::ItemHoverable(bb, 0);
        }

        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();

        ImVec2 textPos = ImVec2(p0.x + style.FramePadding.x, p0.y + style.FramePadding.y);
        auto titleWidth = p1.x - p0.x - style.FramePadding.x * 2;
        m_maxTextSize = Max(m_maxTextSize, textSize.x);
        if (titleWidth < textSize.x)
        {
            static constexpr float kPixelPerSecond = 4.0f;
            static constexpr float kPi = 3.14159265f;
            float period = (m_maxTextSize - titleWidth) / kPixelPerSecond;
            float dt = fmodf(m_textTime, 2.0f + period);
            if (dt <= 1.0f)
                dt = 0.f;
            else if (dt < 1.0f + period * 0.5)
                dt = 2.0f * kPi * (dt - 1.0f) / period;
            else if (dt < 2.0f + period * 0.5)
                dt = kPi;
            else
                dt = 2.0f * kPi * (dt - 2.0f) / period;
            textPos.x -= (textSize.x - titleWidth) * (0.5f - cosf(dt) * 0.5f);
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec4 clip_rect(p0.x + style.FramePadding.x, p0.y, p1.x - style.FramePadding.x, p1.y);
        draw_list->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg));
        std::forward<DrawCallback>(drawCallback)(p0, p1);
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), NULL, 0.0f, &clip_rect);
        return isHoovered;
    }

    void Deck::DrawClippedArtists(MusicID musicId)
    {
        bool isHoovered{ false };
        auto& style = ImGui::GetStyle();

        std::string text = musicId.GetArtists();

        auto textSize = ImGui::CalcTextSize(text.c_str());
        //ImGui::InvisibleButton(label, ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2));
        //it's a hooverable ImGui::Dummy because we want to move the window while dragging this widget
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window->SkipItems)
        {
            auto size = ImGui::CalcItemSize(ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2), 0.0f, 0.0f);
            const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
            ImGui::ItemSize(size);
            ImGui::ItemAdd(bb, 0);
            isHoovered = ImGui::ItemHoverable(bb, 0);
        }

        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();

        ImVec2 textPos = ImVec2(p0.x + style.FramePadding.x, p0.y + style.FramePadding.y);
        auto titleWidth = p1.x - p0.x - style.FramePadding.x * 2;
        m_maxTextSize = Max(m_maxTextSize, textSize.x);
        if (titleWidth < textSize.x)
        {
            static constexpr float kPixelPerSecond = 4.0f;
            static constexpr float kPi = 3.14159265f;
            float period = (m_maxTextSize - titleWidth) / kPixelPerSecond;
            float dt = fmodf(m_textTime, 2.0f + period);
            if (dt <= 1.0f)
                dt = 0.f;
            else if (dt < 1.0f + period * 0.5)
                dt = 2.0f * kPi * (dt - 1.0f) / period;
            else if (dt < 2.0f + period * 0.5)
                dt = kPi;
            else
                dt = 2.0f * kPi * (dt - 2.0f) / period;
            textPos.x -= (textSize.x - titleWidth) * (0.5f - cosf(dt) * 0.5f);
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec4 clip_rect(p0.x + style.FramePadding.x, p0.y, p1.x - style.FramePadding.x, p1.y);
        draw_list->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg));
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), NULL, 0.0f, &clip_rect);

        if (isHoovered)
        {
            auto* song = musicId.GetSong();
            auto mousePos = ImGui::GetMousePos().x;
            for (uint16_t i = 0; i < song->NumArtistIds(); i++)
            {
                auto artist = musicId.GetArtist(song->GetArtistId(i));
                if (i != 0)
                    textPos.x += ImGui::CalcTextSize(" & ").x;
                auto nextTextPos = textPos.x + ImGui::CalcTextSize(artist->GetHandle()).x;
                if (mousePos >= textPos.x && mousePos < nextTextPos)
                {
                    artist->Tooltip();
                    break;
                }
                textPos.x = nextTextPos;
            }
        }
    }

    void Deck::DisplayInfoAndOscilloscope(Player* player)
    {
        auto info = player->GetInfo();
        if (info.empty())
            info = "              /\\\n       <-  rePlayer  ->\n              \\/";
        DrawClippedText(info, [player](const ImVec2& min, const ImVec2& max)
        {
            player->DrawOscilloscope(min.x, min.y, max.x, max.y);
        });
    }

    void Deck::UpdateSystray()
    {
        if (m_isSystrayEnabled)
        {
            m_isSystrayEnabled = false;
            if (m_isSystrayMenuEnabled)
                ImGui::OpenPopup("SystrayMenu");
        }

        ImGuiWindowClass c;
        c.ParentViewportId = 0;
        c.ViewportFlagsOverrideSet = ImGuiViewportFlags_TopMost;

        // todo: for the pivot, detect the position of the taskbar icon
        ImGui::SetNextWindowPos(ImVec2(m_systrayPos[0], m_systrayPos[1]), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowClass(&c);
        if (m_isSystrayBalloonEnabled && !ImGui::IsPopupOpen("SystrayMenu"))
        {
            ImGui::Begin("SystrayBalloon", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

            ImGui::Text("rePlayer %d.%d.%d", Core::GetVersion() >> 28, (Core::GetVersion() >> 14) & ((1 << 14) - 1), Core::GetVersion() & ((1 << 14) - 1));
            ImGui::Separator();
            if (auto* player = m_currentPlayer.Get())
            {
                const auto musicId = player->GetId();
                std::string title = musicId.GetTitle();
                ImGui::TextUnformatted("Title  :");
                ImGui::SameLine();
                ImGui::TextUnformatted(title.c_str());
                std::string artists = musicId.GetArtists();
                ImGui::TextUnformatted( player->GetSong()->artistIds.NumItems() > 1 ? "Artists:" : "Artist :");
                ImGui::SameLine();
                ImGui::TextUnformatted(artists.c_str());
                ImGui::TextUnformatted(player->IsPlaying() ? "Playing:" : player->IsStopped() ? "Stopped:" : "Paused :");
                ImGui::SameLine();
                auto playbackTime = player->GetPlaybackTimeInMs();
                auto duration = Max(1u, player->GetSong()->subsongs[musicId.subsongId.index].durationCs * 10, playbackTime);
                if (m_mode == Mode::Solo)
                {
                    if (m_seekPos >= 0)
                        ImGui::Text("%d:%02d/%d:%02d", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60);
                    else
                        ImGui::Text("%d:%02d/%d:%02d", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60);
                }
                else
                {
                    auto& playlist = Core::GetPlaylist();
                    if (m_seekPos >= 0)
                        ImGui::Text("%d:%02d/%d:%02d [%d/%u]", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
                    else
                        ImGui::Text("%d:%02d/%d:%02d [%d/%u]", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
                }
            }

            ImGui::End();
        }

        // todo: for the pivot, detect the position of the taskbar icon
        ImGui::SetNextWindowPos(ImVec2(m_systrayPos[0], m_systrayPos[1]), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowClass(&c);
        if (ImGui::BeginPopup("SystrayMenu", ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            // Hack begin
            // Imgui doesn't handle properly the active window, so for now, we do it ourself to disable the systray popup when we go to another window not from rePlayer
            if (m_hSystrayWnd == nullptr)
            {
                if (auto hWnd = HWND(ImGui::GetWindowViewport()->PlatformHandle))
                {
                    m_hSystrayWnd = hWnd;
                    ::SetForegroundWindow(hWnd);
                }
            }
            else if (::GetForegroundWindow() != m_hSystrayWnd)
                ImGui::CloseCurrentPopup();
            // Hack end

            if (ImGui::MenuItem("Visible", nullptr, &m_windowStates.isEnabled))
            {
                Enable(m_windowStates.isEnabled);
                m_hSystrayWnd = nullptr;
            }
            ImGui::MenuItem("Always On Top", nullptr, &m_windowStates.isAlwaysOnTop);
            ImGui::MenuItem("Passthrough", nullptr, &m_windowStates.isPassthrough);
            ImGui::MenuItem("Minimal", nullptr, &m_windowStates.isMinimal);
            ImGui::Separator();
            if (ImGui::MenuItem("Invert states"))
                InvertWindowStates();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                m_isOpened = false;

            ImGui::EndPopup();
        }
        else
            m_isSystrayMenuEnabled = false;
    }

    void Deck::InvertWindowStates()
    {
        if (!m_windowStates.isEnabled)
        {
            m_windowStates.isEnabled = true;
            Enable(true);
            m_hSystrayWnd = nullptr;
        }
        else
        {
            m_windowStates.isAlwaysOnTop = !m_windowStates.isAlwaysOnTop;
            m_windowStates.isMinimal = !m_windowStates.isMinimal;
            m_windowStates.isPassthrough = !m_windowStates.isPassthrough;
        }
    }
}
// namespace rePlayer