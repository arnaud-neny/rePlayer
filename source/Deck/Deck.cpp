#include "Deck.h"

#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>

#include <Database/SongEditor.h>
#include <Deck/Patterns.h>
#include <Deck/Player.h>
#include <Library/Library.h>
#include <Playlist/Playlist.h>
#include <Replayer/About.h>
#include <Replayer/Core.h>
#include <Replayer/Replays.h>
#include <Replayer/Settings.h>

#include <windows.h>

#include <cmath>
#include <ctime>

namespace rePlayer
{
    Deck::Deck()
        : Window("System", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking, true)
        , m_patterns(new Patterns)
    {
        Enable(true);
        m_volume = Player::GetVolume();
        Player::SetVolume(m_volume);

        auto hWnd = HWND(ImGui::GetMainViewport()->PlatformHandleRaw);
        ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_NEXTTRACK, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
        ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_PREVIOUSTRACK, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
        ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_STOP, MOD_NOREPEAT, VK_MEDIA_STOP);
        ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_PLAY_PAUSE, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
        ::RegisterHotKey(hWnd, APPCOMMAND_VOLUME_UP, MOD_NOREPEAT, VK_VOLUME_UP);
        ::RegisterHotKey(hWnd, APPCOMMAND_VOLUME_DOWN, MOD_NOREPEAT, VK_VOLUME_DOWN);
    }

    Deck::~Deck()
    {
        delete m_patterns;
    }

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
        else if (m_currentPlayer.IsValid() && !m_currentPlayer->GetId().IsValid())
            m_currentPlayer.Reset();
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
                , ImVec2(x, table->RowPosY2 + tableInstanceData->LastTopHeadersRowHeight + 1)
                , ImGui::GetColorU32(color1));
            draw_list->AddRectFilled(ImVec2(x, table->RowPosY1)
                , ImVec2(table->BorderX2, table->RowPosY2 + tableInstanceData->LastTopHeadersRowHeight + 1)
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
        ImGui::GetIO().FontGlobalScale = m_scale;

        auto windowStates = m_windowStates;
        Window::Update(windowStates);

        return m_isOpened ? Status::kOk : Status::kFail;
    }

    void Deck::DisplaySettings()
    {
        if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Focus current playing song in Playlist", &m_isTrackingSongInPlaylist);
            ImGui::Checkbox("Focus current playing song in Database", &m_isTrackingSongInDatabase);
            uint32_t saveFrequency = (m_saveFrequency + 59) / 60;
            uint32_t saveMin = 0;
            uint32_t saveMax = 24 * 60;
            if (ImGui::SliderScalar("AutoSave", ImGuiDataType_U32, &saveFrequency, &saveMin, &saveMax, "Every %u minutes", ImGuiSliderFlags_AlwaysClamp))
                m_saveFrequency = saveFrequency * 60;
            if (ImGui::Checkbox("Playback media hot keys", &m_arePlaybackMediaHotKeysEnabled))
            {
                auto hWnd = HWND(ImGui::GetMainViewport()->PlatformHandleRaw);
                if (m_arePlaybackMediaHotKeysEnabled)
                {
                    ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_NEXTTRACK, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
                    ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_PREVIOUSTRACK, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
                    ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_STOP, MOD_NOREPEAT, VK_MEDIA_STOP);
                    ::RegisterHotKey(hWnd, APPCOMMAND_MEDIA_PLAY_PAUSE, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
                }
                else
                {
                    ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_NEXTTRACK);
                    ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_PREVIOUSTRACK);
                    ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_STOP);
                    ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_PLAY_PAUSE);
                }
            }
            if (ImGui::Checkbox("Volume media hot keys", &m_areVolumeMediaHotKeysEnabled))
            {
                auto hWnd = HWND(ImGui::GetMainViewport()->PlatformHandleRaw);
                if (m_areVolumeMediaHotKeysEnabled)
                {
                    ::RegisterHotKey(hWnd, APPCOMMAND_VOLUME_UP, MOD_NOREPEAT, VK_VOLUME_UP);
                    ::RegisterHotKey(hWnd, APPCOMMAND_VOLUME_DOWN, MOD_NOREPEAT, VK_VOLUME_DOWN);
                }
                else
                {
                    ::UnregisterHotKey(hWnd, APPCOMMAND_VOLUME_UP);
                    ::UnregisterHotKey(hWnd, APPCOMMAND_VOLUME_DOWN);
                }
            }
#ifdef _WIN64
            ImGui::SliderFloat("Transparency", &m_blendingFactor, 0.25f, 1.0f, "", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
#endif
            ImGui::SliderFloat("Scale", &m_scale, 1.0f, 4.0f, "%1.2f", ImGuiSliderFlags_AlwaysClamp);
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

    inline constexpr Deck::ClippedTextFlags operator&(Deck::ClippedTextFlags a, Deck::ClippedTextFlags b)
    {
        return Deck::ClippedTextFlags(uint8_t(a) & uint8_t(b));
    }

    inline constexpr Deck::ClippedTextFlags operator|(Deck::ClippedTextFlags a, Deck::ClippedTextFlags b)
    {
        return Deck::ClippedTextFlags(uint8_t(a) | uint8_t(b));
    }

    inline constexpr Deck::ClippedTextFlags& operator|=(Deck::ClippedTextFlags& a, Deck::ClippedTextFlags b)
    {
        return a = a | b;
    }

    inline constexpr bool operator!(Deck::ClippedTextFlags a)
    {
        return a == Deck::ClippedTextFlags::kNone;
    }

    void Deck::OnSystray(int32_t x, int32_t y, SystrayState state)
    {
        if (m_isSystrayMenuEnabled)
        {
            if (state == SystrayState::kMouseButtonRight
                || state == SystrayState::kTooltipOpen
                || state == SystrayState::kTooltipClose)
                return;
        }
        if (Core::IsLocked())
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
        m_isSystrayBalloonEnabled &= !m_isSystrayMenuEnabled;
        m_hSystrayWnd = nullptr;
    }

    void Deck::OnBeginUpdate()
    {
        m_windowStates.isEnabled = IsEnabled();
        m_currentMaxTextSize = 0.0f;

        // update the playback
        if (m_currentPlayer.IsValid())
        {
            // we do here some validation because the loaded song can have been updated/deleted...
            bool hasChanged = false;
            bool isPlaying = m_currentPlayer->IsPlaying();
            bool isStopped = m_currentPlayer->IsStopped();
            auto& playlist = Core::GetPlaylist();

            // 1 - check for the playlist file in shelved mode (m_mode should be Mode::Solo)
            if (m_shelvedPlayer.IsValid() && (m_shelvedPlayer->GetSubsong().isDiscarded || m_shelvedPlayer->GetMediaType() != m_shelvedPlayer->GetSong()->type || m_shelvedPlayer->GetId() != playlist.GetCurrentEntry()))
            {
                m_shelvedPlayer = playlist.LoadCurrentSong();
                m_nextPlayer = playlist.LoadNextSong(false);
            }

            // 2 - check solo vs playlist
            if (m_mode == Mode::Solo)
            {
                if (m_currentPlayer->GetMediaType() != m_currentPlayer->GetSong()->type)
                {
                    m_currentPlayer = playlist.LoadSong(m_currentPlayer->GetId());
                    hasChanged = true;
                }
                if (m_currentPlayer.IsInvalid() || m_currentPlayer->GetSubsong().isDiscarded)
                {
                    if (m_shelvedPlayer.IsValid())
                    {
                        m_mode = Mode::Playlist;
                        m_currentPlayer = std::move(m_shelvedPlayer);
                        if (m_nextPlayer.IsValid() && (m_nextPlayer->GetSubsong().isDiscarded || m_nextPlayer->GetMediaType() != m_nextPlayer->GetSong()->type))
                            m_nextPlayer = playlist.LoadNextSong(false);
                        hasChanged = true;
                    }
                    else
                        m_currentPlayer.Reset();
                }
            }
            else if (m_currentPlayer->GetSubsong().isDiscarded || m_currentPlayer->GetMediaType() != m_currentPlayer->GetSong()->type || m_currentPlayer->GetId() != playlist.GetCurrentEntry())
            {
                m_currentPlayer = playlist.LoadCurrentSong();
                m_nextPlayer = playlist.LoadNextSong(false);
                hasChanged = true;
            }
            else if (m_nextPlayer.IsValid() && (m_nextPlayer->GetSubsong().isDiscarded || m_nextPlayer->GetMediaType() != m_nextPlayer->GetSong()->type))
                m_nextPlayer = playlist.LoadNextSong(false);
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
                    else if (isPlaying && m_shelvedPlayer.IsInvalid() && m_nextPlayer.IsValid() && !m_nextPlayer->IsPlaying())
                    {
                        // start the next song without stopping the current one, to have a seamless playback
                        ValidateNextSong();
                        if (m_nextPlayer.IsValid())
                            m_nextPlayer->Play();
                    }
                }
            }
        }

        m_patterns->Update(m_currentPlayer);
    }

    std::string Deck::OnGetWindowTitle()
    {
        SetFlags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize, !m_isExpanded);
        ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
        if (m_isExpanded)
            ImGui::SetNextWindowSizeConstraints(ImVec2(-1.0f, 200.0f), ImVec2(-1.0f, FLT_MAX));
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
        ImGui::BeginChild("VuMeter", ImVec2(24.0f, m_VuMeterHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);//child window to prevent the hover (then tooltip from histogram)
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

            std::string title = player->GetTitle();
            if (!!(DrawClippedText(title) & ClippedTextFlags::kIsHoovered))
            {
                auto metadata = player->GetMetadata();
                if (metadata.size())
                    ImGui::Tooltip(metadata.c_str());
            }

            DrawClippedArtists(player);

            DisplayInfoAndOscilloscope(player);

            auto playbackTime = player->GetPlaybackTimeInMs();
            auto durationCs = song->subsongs[subsongId.index].durationCs;
            auto duration = Max(1u, durationCs * 10, playbackTime);

            char buf[32];
            if (m_mode == Mode::Solo)
            {
                if (m_seekPos >= 0)
                    sprintf(buf, "%u:%02u/%u:%02u", uint32_t(m_seekPos / 60000), uint32_t((m_seekPos / 1000) % 60), duration / 60000, (duration / 1000) % 60);
                else if (durationCs > 0)
                    sprintf(buf, "%u:%02u/%u:%02u", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60);
                else
                    sprintf(buf, "%u:%02u", playbackTime / 60000, (playbackTime / 1000) % 60);
            }
            else
            {
                auto& playlist = Core::GetPlaylist();
                if (m_seekPos >= 0)
                    sprintf(buf, "%u:%02u/%u:%02u [%u/%u]", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60, uint32_t(playlist.GetCurrentEntryIndex() + 1), playlist.NumEntries());
                else if (durationCs > 0)
                    sprintf(buf, "%u:%02u/%u:%02u [%u/%u]", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60, uint32_t(playlist.GetCurrentEntryIndex() + 1), playlist.NumEntries());
                else
                    sprintf(buf, "%u:%02u [%u/%u]", playbackTime / 60000, (playbackTime / 1000) % 60, uint32_t(playlist.GetCurrentEntryIndex() + 1), playlist.NumEntries());
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
            DrawClippedText("            /\\\n     <-  rePlayer  ->\n            \\/");

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

        auto spacing = ImGui::GetStyle().ItemSpacing.x + 2.0f;
        if (ImGui::Button(ImGuiIconMediaPrev))
        {
            PlayPreviousSong();
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button(ImGuiIconMediaStop) && isPlayerValid && !player->IsStopped())
        {
            player->Stop();
            if (m_nextPlayer.IsValid())
                m_nextPlayer->Stop();
        }
        ImGui::SameLine(0.0f, spacing);
        if (!isPlayerValid)
        {
            ImGui::Button(ImGuiIconMediaPlay);
        }
        else if (!player->IsPlaying())
        {
            if (ImGui::Button(ImGuiIconMediaPlay))
                Play();
        }
        else if (ImGui::Button(ImGuiIconMediaPause))
        {
            player->Pause();
            if (m_nextPlayer.IsValid())
                m_nextPlayer->Pause();
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button(ImGuiIconMediaNext))
        {
            PlayNextSong();
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button(m_loop == Loop::None ? ImGuiIconMediaLoopNone : m_loop == Loop::Playlist ? ImGuiIconMediaLoopPlaylist : ImGuiIconMediaLoopSingle))
        {
            m_loop = Loop((m_loop.As<uint8_t>() + 1) % uint8_t(Loop::Count));
            if (m_loop != Loop::Playlist)
            {
                auto isEnabled = m_loop == Loop::Single;
                if (m_currentPlayer.IsValid())
                    m_currentPlayer->EnableEndless(isEnabled);
                if (m_nextPlayer.IsValid())
                    m_nextPlayer->EnableEndless(isEnabled);
                if (m_shelvedPlayer.IsValid())
                    m_shelvedPlayer->EnableEndless(isEnabled);
            }
            ValidateNextSong();
        }
        if (ImGui::IsItemHovered())
            ImGui::Tooltip(m_loop == Loop::None ? "No loop" : m_loop == Loop::Playlist ? "Playlist loop" : "Endless song");
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button(ImGuiIconMediaMenu))
            ImGui::OpenPopup("Windows");
        if (ImGui::BeginPopup("Windows"))
        {
            bool isEnabled = Core::GetSettings().IsEnabled();
            if (ImGui::MenuItem("Settings", "", &isEnabled))
            {
                Core::GetSettings().Enable(isEnabled);
                if (isEnabled && player)
                    Core::GetReplays().SetSelectedSettings(player->GetMediaType().replay);
            }
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
            isEnabled = m_patterns->IsEnabled();
            if (ImGui::MenuItem("Patterns Display", "", &isEnabled))
                m_patterns->Enable(isEnabled);
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
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button(m_isExpanded ? "^" : "v"))
        {
            m_isExpanded = !m_isExpanded;
        }
        ImGui::EndGroup();
        m_VuMeterHeight = ImGui::GetItemRectSize().y;

        if (m_isExpanded)
        {
            auto window = ImGui::GetCurrentWindow();
            window->AutoFitFramesX = 2;

            if (ImGui::BeginChild("Metadata", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border, 0))
            {
                auto metadata = isPlayerValid ? player->GetMetadata() : "";
                // input text so we can select and copy text
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_ChildBg));
                ImGui::InputTextMultiline("##Metadata", const_cast<char*>(metadata.c_str()), metadata.size() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
        }
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
        else if (m_currentMaxTextSize != m_maxTextSize)
            m_maxTextSize = m_currentMaxTextSize;
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

        auto hWnd = HWND(ImGui::GetMainViewport()->PlatformHandleRaw);
        if (!m_arePlaybackMediaHotKeysEnabled)
        {
            ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_NEXTTRACK);
            ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_PREVIOUSTRACK);
            ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_STOP);
            ::UnregisterHotKey(hWnd, APPCOMMAND_MEDIA_PLAY_PAUSE);
        }
        if (!m_areVolumeMediaHotKeysEnabled)
        {
            ::UnregisterHotKey(hWnd, APPCOMMAND_VOLUME_UP);
            ::UnregisterHotKey(hWnd, APPCOMMAND_VOLUME_DOWN);
        }
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
    Deck::ClippedTextFlags Deck::DrawClippedText(const std::string& text, bool isScrolling, DrawCallback&& drawCallback)
    {
        ClippedTextFlags flags = ClippedTextFlags::kNone;
        auto& style = ImGui::GetStyle();

        auto textSize = ImGui::CalcTextSize(text.c_str());
        if (isScrolling)
            m_currentMaxTextSize = Max(m_currentMaxTextSize, textSize.x);
        //ImGui::InvisibleButton(label, ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2));
        //it's a hooverable ImGui::Dummy because we want to move the window while dragging this widget
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window->SkipItems)
        {
            auto size = ImGui::CalcItemSize(ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2), 0.0f, 0.0f);
            const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
            ImGui::ItemSize(size);
            ImGui::ItemAdd(bb, 0);
            if (ImGui::ItemHoverable(bb, 0, 0))
                flags |= ClippedTextFlags::kIsHoovered;
        }

        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();

        ImVec2 textPos = ImVec2(p0.x + style.FramePadding.x, p0.y + style.FramePadding.y);
        auto titleWidth = p1.x - p0.x - style.FramePadding.x * 2;
        if (titleWidth < textSize.x)
            flags |= ClippedTextFlags::kIsClipped;
        if (isScrolling)
        {
            m_maxTextSize = Max(m_maxTextSize, textSize.x);
            if (!!(flags & ClippedTextFlags::kIsClipped))
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
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec4 clip_rect(p0.x + style.FramePadding.x, p0.y, p1.x - style.FramePadding.x, p1.y);
        draw_list->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg));
        std::forward<DrawCallback>(drawCallback)(p0, p1);
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), NULL, 0.0f, &clip_rect);
        return flags;
    }

    void Deck::DrawClippedArtists(Player* player)
    {
        bool isHoovered = false;
        auto& style = ImGui::GetStyle();

        std::string text = player->GetArtists();

        auto textSize = ImGui::CalcTextSize(text.c_str());
        m_currentMaxTextSize = Max(m_currentMaxTextSize, textSize.x);
        //ImGui::InvisibleButton(label, ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2));
        //it's a hooverable ImGui::Dummy because we want to move the window while dragging this widget
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window->SkipItems)
        {
            auto size = ImGui::CalcItemSize(ImVec2(-1.0f, textSize.y + style.FramePadding.y * 2), 0.0f, 0.0f);
            const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
            ImGui::ItemSize(size);
            ImGui::ItemAdd(bb, 0);
            isHoovered = ImGui::ItemHoverable(bb, 0, 0);
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
            auto musicId = player->GetId();
            auto* song = player->GetSong();
            auto mousePos = ImGui::GetMousePos().x;
            for (uint16_t i = 0; i < song->artistIds.NumItems(); i++)
            {
                auto artist = musicId.GetArtist(song->artistIds[i]);
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
            info = "            /\\\n     <-  rePlayer  ->\n            \\/";
        auto flags = DrawClippedText(info, false, [player](const ImVec2& min, const ImVec2& max)
        {
            player->DrawVisuals(min.x, min.y, max.x, max.y);
        });
        if (flags == (ClippedTextFlags::kIsClipped | ClippedTextFlags::kIsHoovered))
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(info.c_str(), info.c_str() + info.size());
            ImGui::EndTooltip();
        }
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

            ImGui::TextUnformatted(Core::GetLabel());
            ImGui::Separator();
            if (auto* player = m_currentPlayer.Get())
            {
                const auto musicId = player->GetId();
                std::string title = player->GetTitle();
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
                auto durationCs = player->GetSong()->subsongs[musicId.subsongId.index].durationCs;
                auto duration = Max(1u, durationCs * 10, playbackTime);
                if (m_mode == Mode::Solo)
                {
                    if (m_seekPos >= 0)
                        ImGui::Text("%d:%02d/%d:%02d", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60);
                    else if (durationCs > 0)
                        ImGui::Text("%d:%02d/%d:%02d", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60);
                    else
                        ImGui::Text("%d:%02d", playbackTime / 60000, (playbackTime / 1000) % 60);
                }
                else
                {
                    auto& playlist = Core::GetPlaylist();
                    if (m_seekPos >= 0)
                        ImGui::Text("%d:%02d/%d:%02d [%d/%u]", m_seekPos / 60000, (m_seekPos / 1000) % 60, duration / 60000, (duration / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
                    else if (durationCs > 0)
                        ImGui::Text("%d:%02d/%d:%02d [%d/%u]", playbackTime / 60000, (playbackTime / 1000) % 60, duration / 60000, (duration / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
                    else
                        ImGui::Text("%d:%02d [%d/%u]", playbackTime / 60000, (playbackTime / 1000) % 60, playlist.GetCurrentEntryIndex() + 1, playlist.NumEntries());
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