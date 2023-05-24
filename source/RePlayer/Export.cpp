#include "Export.h"

// core
#include <Core/String.h>
#include <IO/File.h>

// rePlayer
#include <Deck/Player.h>
#include <Library/Library.h>
#include <Playlist/Playlist.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

// dr_wav
#define DR_WAV_IMPLEMENTATION
#define DRWAV_MALLOC(sz) core::Alloc((sz))
#define DRWAV_REALLOC(p, sz) core::Realloc((p), (sz))
#define DRWAV_FREE(p) core::Free((p))
#include <dr_wav.h>

// Windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// stl
#include <atomic>

namespace rePlayer
{
    Export::Export()
    {}

    Export::~Export()
    {
        if (m_threadHandle != 0)
        {
            ::WaitForSingleObject(m_threadHandle, INFINITE);
            ::CloseHandle(m_threadHandle);
        }
    }

    void Export::Enqueue(MusicID musicId)
    {
        auto song = musicId.GetSong();
        m_songs.Add({ song, song->Edit(), musicId });
    }

    bool Export::Start()
    {
        if (m_songs.IsEmpty())
            return false;

        m_threadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, this, 0, nullptr);
        if (m_threadHandle == 0)
        {
            Update();
            return false;
        }
        return true;
    }

    void Export::GetStatus(MusicID& musicId, float& progress, uint32_t& duration) const
    {
        auto currentEntry = std::atomic_ref(m_currentEntry).load();
        musicId = m_songs[currentEntry].id;
        progress = std::atomic_ref(m_progress).load();
        duration = std::atomic_ref(m_duration).load();
    }

    bool Export::IsDone()
    {
        return ::WaitForSingleObject(m_threadHandle, 0) != WAIT_TIMEOUT;
    }

    uint32_t Export::ThreadFunc(uint32_t* lpdwParam)
    {
        auto* This = reinterpret_cast<Export*>(lpdwParam);
        This->Update();
        return 0;
    }

    void Export::Update()
    {
        for (uint32_t i = 0, e = m_songs.NumItems(); i < e; i++)
        {
            std::atomic_ref(m_progress).store(0.0f);
            std::atomic_ref(m_duration).store(0);
            std::atomic_ref(m_currentEntry).store(i);

            auto& entry = m_songs[i];

            SmartPtr<core::io::Stream> stream;
            if (entry.id.databaseId == DatabaseID::kPlaylist)
                stream = Core::GetPlaylist().GetStream(entry.song);
            else
                stream = Core::GetLibrary().GetStream(entry.song);
            if (auto replay = Core::GetReplays().Load(stream, entry.songSheet->metadata.Container(), entry.songSheet->type))
            {
                auto player = Player::Create(entry.id, entry.songSheet, replay, stream, true);

                auto maxFrames = (uint64_t(entry.songSheet->subsongs[entry.id.subsongId.index].durationCs) * replay->GetSampleRate()) / 100ull;

                auto filename = entry.id.GetArtists();
                if (filename.empty())
                    filename = "!Unknown! - ";
                else
                    filename += " - ";
                filename += entry.songSheet->name.Items();
                filename += " [";
                if (*entry.songSheet->subsongs[entry.id.subsongId.index].name.Items())
                {
                    filename += entry.songSheet->name.Items();
                    filename += "][";
                }
                char buf[32];
                sprintf(buf, "%08X%c].wav", entry.id.subsongId.value, entry.id.databaseId == DatabaseID::kPlaylist ? 'p' : 'l');
                filename += buf;
                io::File::CleanFilename(filename.data());

                drwav wav;
                drwav_data_format format;
                format.container = drwav_container_riff;
                format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
                format.channels = 2;
                format.sampleRate = replay->GetSampleRate();
                format.bitsPerSample = 32;
                if (!drwav_init_file_write(&wav, filename.c_str(), &format, nullptr))
                    continue;

                for(;;)
                {
                    auto currentPos = player->m_songPos;
                    player->Render(player->m_numSamples, 0);
                    if (player->m_songEnd != ~0ull)
                    {
                        drwav_write_pcm_frames(&wav, currentPos - player->m_songEnd, player->m_waveData);
                        std::atomic_ref(m_progress).store(1.0f);
                        break;
                    }
                    auto framesWritten = drwav_write_pcm_frames(&wav, player->m_numSamples, player->m_waveData);
                    if (maxFrames != 0)
                        std::atomic_ref(m_progress).store(float((double((player->m_songPos << 32) / maxFrames) / 65536.0) / 65536.0));
                    if (framesWritten != player->m_numSamples)
                        break;
                    std::atomic_ref(m_duration).store(uint32_t(player->m_songPos / format.sampleRate));
                }
                std::atomic_ref(m_duration).store(0xffFFffFF);
                drwav_uninit(&wav);
            }
        }
    }
}
// namespace rePlayer