#include "Core.h"

// Core
#include <Core/Log.h>

// rePlayer
#include <Database/Database.h>
#include <Database/SongEditor.h>
#include <Deck/Deck.h>
#include <Library/Library.h>
#include <Playlist/Playlist.h>
#include <RePlayer/About.h>
#include <RePlayer/Replays.h>
#include <RePlayer/Settings.h>

// curl
#include <curl/curl.h>

// stl
#include <atomic>

namespace rePlayer
{
    RePlayer* Core::Create()
    {
        if (ms_instance)
            return nullptr;

        curl_global_init_mem(CURL_GLOBAL_DEFAULT
            , [](size_t size) { return core::Alloc(size); }
        , [](void* ptr) { core::Free(ptr); }
        , [](void* ptr, size_t size) { return core::Realloc(ptr, size); }
        , [](const char* str) { char* ptr = nullptr; if (str) { auto size = strlen(str); ptr = core::Alloc<char>(size + 1); memcpy(ptr, str, size + 1); } return ptr; }
        , [](size_t nmemb, size_t size) { void* ptr = core::Alloc(nmemb * size); memset(ptr, 0, nmemb * size); return ptr; });

        if (CheckForNewVersion() == Status::kOk)
            ms_instance = new Core();
        else
            curl_global_cleanup();

        struct
        {
            static LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* e)
            {
                if (ms_instance)
                {
                    static std::atomic<int32_t> isLock = 0;
                    if (isLock.exchange(1) == 0)
                    {
                        Log::Error("CRASH: %08X\n", e->ExceptionRecord->ExceptionCode);
                        ms_instance->m_library->Save();
                        ms_instance->m_playlist->Save();
                    }
                }
                return EXCEPTION_CONTINUE_SEARCH;
            }
        } cb;
        ::SetUnhandledExceptionFilter(&cb.TopLevelExceptionFilter);

        return ms_instance;
    }

    Core::~Core()
    {
        m_library->Save();
        m_playlist->Save();

        delete m_deck;
        delete m_settings;
        delete m_replays;
        delete m_songEditor;
        delete m_library;
        delete m_playlist;
        delete m_about;
        for (auto* db : m_db)
            delete db;
    }

    void Core::Release()
    {
        if (ms_instance)
        {
            delete this;

            curl_global_cleanup();

            ms_instance = nullptr;
        }
    }

    Status Core::Launch()
    {
        if (m_deck == nullptr)
        {
            for (auto& db : m_db)
                db = new Database();
            m_deck = new Deck();
            m_settings = new Settings();
            m_replays = new Replays();
            m_songEditor = new SongEditor();
            m_library = new Library();
            m_playlist = new Playlist();
            m_about = new About();
        }

        return Status::kOk;
    }

    Status Core::UpdateFrame()
    {
        Reconcile();

        return m_deck->UpdateFrame();
    }

    void Core::Enable(bool isEnabled)
    {
        m_deck->Enable(isEnabled);
    }

    float Core::GetBlendingFactor() const
    {
        return m_deck->GetBlendingFactor();
    }

    void Core::PlayPause()
    {
        m_deck->PlayPause();
    }

    void Core::Next()
    {
        m_deck->Next();
    }

    void Core::Previous()
    {
        m_deck->Previous();
    }

    void Core::Stop()
    {
        m_deck->Stop();
    }

    void Core::IncreaseVolume()
    {
        m_deck->IncreaseVolume();
    }

    void Core::DecreaseVolume()
    {
        m_deck->DecreaseVolume();
    }

    void Core::SystrayMouseLeft(int32_t x, int32_t y)
    {
        m_deck->OnSystray(x, y, Deck::SystrayState::kMouseButtonLeft);
    }

    void Core::SystrayMouseMiddle(int32_t x, int32_t y)
    {
        m_deck->OnSystray(x, y, Deck::SystrayState::kMouseButtonMiddle);
    }

    void Core::SystrayMouseRight(int32_t x, int32_t y)
    {
        m_deck->OnSystray(x, y, Deck::SystrayState::kMouseButtonRight);
    }

    void Core::SystrayTooltipOpen(int32_t x, int32_t y)
    {
        m_deck->OnSystray(x, y, Deck::SystrayState::kTooltipOpen);
    }

    void Core::SystrayTooltipClose(int32_t x, int32_t y)
    {
        m_deck->OnSystray(x, y, Deck::SystrayState::kTooltipClose);
    }
}
// namespace rePlayer