// core
#include <Core/String.h>
#include <Thread/Thread.h>

// rePlayer
#include <Database/Database.h>
#include <Deck/Player.h>
#include <Playlist/Playlist.h>
#include <RePlayer/Core.h>

#include "ReplayGainScanner.h"

#include <ebur128/ebur128.h>

// stl
#include <atomic>

namespace rePlayer
{
    ReplayGainScanner::ReplayGainScanner()
    {}

    ReplayGainScanner::~ReplayGainScanner()
    {
        while (!std::atomic_ref(m_isJobDone).load())
            thread::Sleep(0);
    }

    void ReplayGainScanner::Enqueue(MusicID musicId)
    {
        auto song = musicId.GetSong();
        m_songs.Add({ song, song->Edit(), musicId });
    }

    bool ReplayGainScanner::Start()
    {
        if (m_songs.IsEmpty())
            return false;

        Core::AddJob([this]()
        {
            Update();
        });
        return true;
    }

    void ReplayGainScanner::Cancel()
    {
        std::atomic_ref(m_isCancelled).store(true);
    }

    uint32_t ReplayGainScanner::GetStatus(float& progress, uint32_t& duration) const
    {
        auto currentEntry = std::atomic_ref(m_currentEntry).load();
        progress = std::atomic_ref(m_progress).load();
        duration = std::atomic_ref(m_duration).load();
        return currentEntry;
    }

    bool ReplayGainScanner::IsDone()
    {
        return std::atomic_ref(m_isJobDone).load();
    }

    void ReplayGainScanner::Update()
    {
        Array<Entry> songs;
        for (uint32_t i = 0; i < m_songs.NumItems() && !IsCancelled(); i++)
        {
            std::atomic_ref(m_currentEntry).store(i);
            songs.Add(m_songs[i]);
            do
            {
                std::atomic_ref(m_progress).store(0.0f);
                std::atomic_ref(m_duration).store(0);

                auto entry = songs.Pop();

                auto lastSubsongIndex = entry.songSheet->lastSubsongIndex;

                auto player = Core::GetPlaylist().LoadSong(entry.id, true);
                if (player.IsValid())
                {
                    auto* replay = player->m_replay;
                    if (replay->IsStreaming() || entry.id.subsongId.index > entry.songSheet->lastSubsongIndex)
                    {
                        // - we won't export stream as it never ends and works only realtime
                        // - it's possible the song has been updated and the current subsong doesn't exist anymore
                        continue;
                    }
                    // check if the subsongs have changed and add it to the queue
                    for (++lastSubsongIndex; lastSubsongIndex < entry.songSheet->lastSubsongIndex; ++lastSubsongIndex)
                    {
                        songs.Add(entry);
                        songs.Last().id.subsongId.index = lastSubsongIndex;
                    }

                    uint64_t maxFrames = UINT64_MAX;
                    auto duration = (uint64_t(replay->GetDurationMs()) * replay->GetSampleRate()) / 1000ull;
                    if (duration == 0)
                        maxFrames = 20ull * 60ull * replay->GetSampleRate();
                    replay->ResetPlayback();

                    ebur128_state* st = ebur128_init(2, replay->GetSampleRate(), EBUR128_MODE_I | EBUR128_MODE_S | EBUR128_MODE_TRUE_PEAK);

                    uint64_t position = 0;
                    for (; !IsCancelled();)
                    {
                        auto numSamples = replay->Render(player->m_waveData, player->m_numSamples);
                        if (numSamples == 0 || position > maxFrames)
                        {
                            double lPeak = 0.0, rPeak = 0.0;
                            ebur128_true_peak(st, 0, &lPeak);
                            ebur128_true_peak(st, 1, &rPeak);
                            entry.songSheet->subsongs[entry.id.subsongId.index].replayGain.peak = float(Max(lPeak, rPeak));
                            double loudness = 0.0;
                            ebur128_loudness_global(st, &loudness);
                            auto oldRG = rcCast<uint32_t>(entry.songSheet->subsongs[entry.id.subsongId.index].replayGain.gain);
                            if (loudness != -HUGE_VAL)
                                entry.songSheet->subsongs[entry.id.subsongId.index].replayGain.gain = float(-18.0 - loudness);
                            else
                                entry.songSheet->subsongs[entry.id.subsongId.index].replayGain.Invalidate();
                            if (oldRG != rcCast<uint32_t>(entry.songSheet->subsongs[entry.id.subsongId.index].replayGain.gain))
                                Core::GetDatabase(entry.id.databaseId).Raise(Database::Flag::kSaveSongs);

                            std::atomic_ref(m_progress).store(1.0f);
                            break;
                        }
                        ebur128_add_frames_float(st, pcCast<float>(player->m_waveData), numSamples);
                        position += numSamples;
                        if (duration != 0)
                            std::atomic_ref(m_progress).store(Saturate(float((double((position << 32) / duration) / 65536.0) / 65536.0)));
                        else
                            std::atomic_ref(m_progress).store(Saturate(float((double((position << 32) / maxFrames) / 65536.0) / 65536.0)));
                        std::atomic_ref(m_duration).store(uint32_t(position / replay->GetSampleRate()));
                    }
                    std::atomic_ref(m_duration).store(0xffFFffFF);
                    ebur128_destroy(&st);
                }
            } while (songs.IsNotEmpty() && !IsCancelled());
        }
        std::atomic_ref(m_isJobDone).store(true);
    }

    bool ReplayGainScanner::IsCancelled() const
    {
        return std::atomic_ref(m_isCancelled).load();
    }
}
// namespace rePlayer