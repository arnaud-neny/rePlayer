#pragma once

#include <Database/Types/MusicID.h>
#include <RePlayer/RePlayer.h>

namespace rePlayer
{
    using namespace core;

    class About;
    class Database;
    class Deck;
    class Library;
    class Playlist;
    class Replays;
    class Settings;
    class SongEditor;

    class Core : public ::RePlayer
    {
    public:
        static RePlayer* Create();

        // Commands?
//         Replay* Load(MusicID musicId);
        static void Enqueue(MusicID musicId);
        static void Discard(MusicID musicId);

        // Lock system to prevent issues with imgui and multi-window
        // (opening another pop-up while a modal is opened is closing the modal)
        static void Lock();
        static void Unlock();
        static bool IsLocked();

        // Jukebox
        static About& GetAbout();
        static Database& GetDatabase(DatabaseID databaseId);
        static Deck& GetDeck();
        static Library& GetLibrary();
        static Playlist& GetPlaylist();
        static Replays& GetReplays();
        static Settings& GetSettings();
        static SongEditor& GetSongEditor();

        // Proxy only
        template <typename ItemID>
        static void OnNewProxy(ItemID id);

        // Misc
        static uint32_t GetVersion();
        static const char* const GetLabel();
        static constexpr uint16_t kReferenceDate = 19473; // std::chrono::year(2023) / 4 / 26;

    private:
        template <typename ItemID>
        struct Stack
        {
            Array<ItemID> items;

            void Reconcile();
        };

    private:
        Core() = default;
        ~Core() override;

        void Release() override;

        core::Status Launch() override;
        core::Status UpdateFrame() override;

        void Enable(bool isEnabled) override;
        float GetBlendingFactor() const override;

        // Media
        void PlayPause() override;
        void Next() override;
        void Previous() override;
        void Stop() override;
        void IncreaseVolume() override;
        void DecreaseVolume() override;

        // Systray
        void SystrayMouseLeft(int32_t x, int32_t y) override;
        void SystrayMouseMiddle(int32_t x, int32_t y) override;
        void SystrayMouseRight(int32_t x, int32_t y) override;
        void SystrayTooltipOpen(int32_t x, int32_t y) override;
        void SystrayTooltipClose(int32_t x, int32_t y) override;

        static Status CheckForNewVersion();
        void Reconcile();

    private:
        About* m_about = nullptr;
        Deck* m_deck = nullptr;
        Library* m_library = nullptr;
        Playlist* m_playlist = nullptr;
        Replays* m_replays = nullptr;
        Settings* m_settings = nullptr;
        SongEditor* m_songEditor = nullptr;
        Database* m_db[int32_t(rePlayer::DatabaseID::kCount)];

        Stack<SongID> m_songsStack;
        Stack<ArtistID> m_artistsStack;

        uint32_t m_isLocked = 0;

        static Core* ms_instance;
    };
}
// namespace rePlayer

#define ImGuiIconMediaPrev "\xee\x80\x80"
#define ImGuiIconMediaNext "\xee\x80\x81"
#define ImGuiIconMediaStop "\xee\x80\x82"
#define ImGuiIconMediaPlay "\xee\x80\x83"
#define ImGuiIconMediaPause "\xee\x80\x84"
#define ImGuiIconMediaLoopNone "\xee\x80\x85"
#define ImGuiIconMediaLoopPlaylist "\xee\x80\x86"
#define ImGuiIconMediaLoopSingle "\xee\x80\x87"
#define ImGuiIconMediaMenu "\xee\x80\x88"

#include "Core.inl.h"