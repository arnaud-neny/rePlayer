#pragma once

#include <Containers/SmartPtr.h>
#include <Core/Window.h>
#include <Database/Types/MusicID.h>

struct ImVec2;

namespace rePlayer
{
    using namespace core;

    class Player;

    class Deck : public Window
    {
    public:
        enum class SystrayState
        {
            kMouseButtonLeft,
            kMouseButtonMiddle,
            kMouseButtonRight,
            kTooltipOpen,
            kTooltipClose
        };

    public:
        Deck();
        ~Deck();

        void PlaySolo(MusicID musicId);
        void PlaySolo(const SmartPtr<Player>& player);
        void Play(const SmartPtr<Player>& player1, const SmartPtr<Player>& player2);

        void PlayPause();
        void Next();
        void Previous();
        void Stop();
        void IncreaseVolume();
        void DecreaseVolume();

        bool IsLooping() const { return m_isLooping; }
        bool IsSolo() const { return m_mode == Mode::Solo; }

        void OnNewPlaylist();

        void DisplayProgressBarInTable(float backgroundRatio = 0.0f);

        MusicID GetCurrentPlayerId() const;
        std::string GetMetadata(const MusicID musicId) const;

        Status UpdateFrame();

        void DisplaySettings();
        void ChangedSettings();

        void OnSystray(int32_t x, int32_t y, SystrayState state);

        float GetBlendingFactor() const { return m_blendingFactor; }

    private:
        enum class Tracking : bool
        {
            IsDisabled = false,
            IsEnabled = true
        };

    private:
        void OnBeginUpdate() override;
        std::string OnGetWindowTitle() override;
        void OnDisplay() override;
        void OnEndUpdate() override;
        void OnEndFrame() override;
        void OnApplySettings() override;

        void PlayPreviousSong();
        void PlayNextSong();
        void ValidateNextSong();

        void Play(Tracking isTrackingEnabled = Tracking::IsEnabled);

        template <typename DrawCallback>
        bool DrawClippedText(const std::string& text, DrawCallback&& drawCallback);
        bool DrawClippedText(const std::string& text) { return DrawClippedText(text, [](const ImVec2& /*min*/, const ImVec2& /*max*/) {}); }
        void DrawClippedArtists(MusicID musicId);

        void DisplayInfoAndOscilloscope(Player* player);

        void UpdateSystray();
        void InvertWindowStates();

    private:
        SmartPtr<Player> m_currentPlayer;
        SmartPtr<Player> m_nextPlayer;
        SmartPtr<Player> m_shelvedPlayer;

        SubsongID m_currentSubsongId;

        enum class Mode : uint8_t
        {
            Solo,
            Playlist
        } m_mode = Mode::Solo;
        Serialized<bool> m_isLooping = { "Loop", false };
        Serialized<bool> m_isTrackingSongInPlaylist = { "TrackPlaylist", true };
        Serialized<bool> m_isTrackingSongInDatabase = { "TrackDatabase", true };
        Serialized<bool> m_isExpanded = { "IsExpanded", false };
        bool m_isSystrayEnabled = false;
        bool m_isSystrayMenuEnabled = false;
        bool m_isSystrayBalloonEnabled = false;
        bool m_isOpened = true;

        Serialized<int32_t> m_volume = { "Volume", 0xffFF };
        float m_VuMeterHeight = 100.f;

        Serialized<float> m_blendingFactor = { "Blending", 0.8f };

        int64_t m_seekPos = -1;

        Serialized<uint32_t> m_saveFrequency = { "AutoSave", 15 * 60 };
        float m_saveTime = 0.0f;

        float m_maxTextSize = 0.0f;
        float m_textTime = 0.0f;
        float m_systrayPos[2];
        void* m_hSystrayWnd = nullptr;
        Window::States m_windowStates;

        Serialized<bool> m_arePlaybackMediaHotKeysEnabled = { "PlaybackMediaHotKeys", true };
        Serialized<bool> m_areVolumeMediaHotKeysEnabled = { "VolumeMediaHotKeys", true };
    };
}
// namespace rePlayer