#pragma once

#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>
#include <Database/Types/MusicID.h>
#include <Replays/Replay.h>
#include <Thread/Semaphore.h>

namespace rePlayer
{
    class Player : public RefCounted
    {
        friend class SmartPtr<Player>;
        friend class Export;
    public:
        enum EndingState
        {
            kEnded = -1,
            kNotEnding = 0,
            kEnding = 1
        };

    public:
        static SmartPtr<Player> Create(MusicID id, SongSheet* song, Replay* replay, io::Stream* stream, bool isExport = false);

        void Play();
        void Pause();
        void Stop();
        void Seek(uint32_t timeInMs);

        void MarkSongAsNew(bool isNewSong);
        bool IsNewSong() const;

        void ApplySettings();

        bool IsStopped() const;
        bool IsPlaying() const;
        EndingState IsEnding(uint32_t timeRangeInMs) const;

        bool IsSeekable() const;

        MusicID GetId() const;
        SongSheet* GetSong() const;
        const SubsongSheet& GetSubsong() const;
        uint32_t GetPlaybackTimeInMs() const;
        std::string GetTitle() const;
        std::string GetArtists() const;
        std::string GetMetadata() const;
        std::string GetInfo() const;
        MediaType GetMediaType() const;

        StereoSample GetVuMeter() const;
        void DrawOscilloscope(float xMin, float yMin, float xMax, float yMax) const;

        static uint32_t GetVolume();
        static void SetVolume(uint32_t volume);

        void EnableEndless(bool isEnabled);

    private:
        Player(MusicID id, SongSheet* song, Replay* replay);
        ~Player() override;

        bool Init(io::Stream* stream, bool isExport);

        void ThreadUpdate();

        void Render(uint32_t numSamples, uint32_t waveFillPos);
        void ResumeThread();
        void SuspendThread();

    private:
        struct Wave;

    private:
        MusicID m_id;
        SmartPtr<SongSheet> m_song;
        Replay* m_replay;

        Wave* m_wave = nullptr;

        thread::Semaphore m_semaphore;

        StereoSample* m_waveData = nullptr;
        uint64_t m_songEnd = ~0ull;
        uint64_t m_songSeek = 0;
        uint64_t m_songPos = 0;
        uint32_t m_wavePlayPos = 0;
        uint32_t m_waveFillPos = 0;
        const uint32_t m_numSamples;
        const uint32_t m_numCachedSamples;

        int32_t m_numLoops = 0;
        uint32_t m_remainingFadeOut;
        uint32_t m_fadeOutSilence = 0;
        float m_fadeOutRatio;
        bool m_isWaiting = true;
        bool m_isRunning = true;
        bool m_isJobDone = false;
        bool m_isNewSong = false;
        bool m_hasSeeked = false;
        enum class Status : uint8_t
        {
            Stopped,
            Paused,
            Playing
        } m_status;

        std::string m_extraInfo;
    };
}
// namespace rePlayer

#include "Player.inl"