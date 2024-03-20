// core
#include <Core/String.h>
#include <IO/File.h>
#include <Thread/Thread.h>

// rePlayer
#include <Deck/Player.h>
#include <Library/Library.h>
#include <Playlist/Playlist.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

#include "Export.h"

// dr_wav
#define DR_WAV_IMPLEMENTATION
#define DRWAV_MALLOC(sz) core::Alloc((sz))
#define DRWAV_REALLOC(p, sz) core::Realloc((p), (sz))
#define DRWAV_FREE(p) core::Free((p))
#include <dr_wav.h>

// stl
#include <atomic>

namespace rePlayer
{
    Export::Export()
    {}

    Export::~Export()
    {
        while (!std::atomic_ref(m_isJobDone).load())
            thread::Sleep(0);
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

        Core::AddJob([this]()
        {
            Update();
            std::atomic_ref(m_isJobDone).store(true);
        });
        return true;
    }

    void Export::Cancel()
    {
        std::atomic_ref(m_isCancelled).store(true);
    }

    uint32_t Export::GetStatus(float& progress, uint32_t& duration) const
    {
        auto currentEntry = std::atomic_ref(m_currentEntry).load();
        progress = std::atomic_ref(m_progress).load();
        duration = std::atomic_ref(m_duration).load();
        return currentEntry;
    }

    bool Export::IsDone()
    {
        return std::atomic_ref(m_isJobDone).load();
    }

    void Export::Update()
    {
        for (uint32_t i = 0, e = m_songs.NumItems(); i < e && !IsCancelled(); i++)
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
                if (replay->IsStreaming())
                {
                    // we won't export stream as it never ends and works only realtime
                    delete replay;
                    continue;
                }
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
                sprintf(buf, "%016llX%c].wav", entry.id.subsongId.Value(), entry.id.databaseId == DatabaseID::kPlaylist ? 'p' : 'l');
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

                for(; !IsCancelled();)
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

    bool Export::IsCancelled() const
    {
        return std::atomic_ref(m_isCancelled).load();
    }
}
// namespace rePlayer