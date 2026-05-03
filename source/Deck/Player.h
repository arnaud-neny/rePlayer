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
        friend class ReplayGainScanner;
    public:
        enum EndingState
        {
            kEnded = -1,
            kNotEnding = 0,
            kEnding = 1
        };

        enum class CrossFadeState
        {
            kNone = 0,
            kCrossFadeIn = 1 << 0,
            kCrossFadeOut = 1 << 1,
            kCrossFadeInOut = kCrossFadeIn | kCrossFadeOut
        };

    public:
        static SmartPtr<Player> Create(MusicID id, SongSheet* song, Replay* replay, io::Stream* stream, bool isExport = false);

        void Play(CrossFadeState crossFadeState);
        void Pause();
        void Stop();
        void Seek(uint32_t timeInMs);
        void SetSubsong(uint16_t subsongIndex);

        void MarkSongAsNew(bool isNewSong);
        bool IsNewSong() const;

        void ApplySettings();

        bool IsStopped() const;
        bool IsPlaying() const;
        EndingState IsEnding() const;

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

        Replay::Patterns GetPatterns(uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Replay::Patterns::Flags flags) const;
        const Replay::Properties& GetProperties() const;

        StereoSample GetVuMeter() const;
        void DrawVisuals(float xMin, float yMin, float xMax, float yMax) const;

        static uint32_t GetVolume(bool isLogarithmic);
        static void SetVolume(uint32_t volume, bool isLogarithmic);

        void EnableEndless(bool isEnabled);

    private:
        Player(MusicID id, SongSheet* song, Replay* replay);
        ~Player() override;

        bool Init(io::Stream* stream, bool isExport);

        uint32_t GetPlayingPosition() const;

        void ThreadUpdate();

        void FillCache();
        bool Render(uint32_t numSamples, uint32_t waveFillPos);
        uint32_t Generate(uint32_t numSamples, uint32_t waveFillPos);
        void Scale(uint32_t numSamples, uint32_t waveFillPos);
        void Crossfade(uint32_t numSamples, uint32_t waveFillPos);
        void ResumeThread();
        void SuspendThread();

        void DrawOscilloscope(float xMin, float yMin, float xMax, float yMax) const;
        void DrawPatterns(float xMin, float yMin, float xMax, float yMax) const;

        void SetupCrossFade(CrossFadeState crossFadeState);

    private:
        struct Wave;

        static constexpr uint32_t kCharWidth = 3;
        static constexpr uint32_t kCharHeight = 5;
        static constexpr int kNumRetries = 4;

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
        mutable uint32_t m_patternsPos = 0;
        const uint32_t m_numSamples;

        uint32_t m_crossFadePosition = 0;
        uint32_t m_crossFadeOut = 0;
        uint32_t m_crossFadeInLength = 0;
        uint32_t m_crossFadeOutLength = 0;

        int32_t m_numLoops = 0;
        uint32_t m_remainingFadeOut;
        uint32_t m_fadeOutSilence = 0;
        float m_fadeOutRatio;
        uint32_t m_waitTime = 0;
        int m_numRetries = kNumRetries;
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
        } m_status = Status::Stopped;

        std::string m_extraInfo;
        Replay::Properties m_properties;

    public:
        static bool ms_isReplayGainEnabled;
        static bool ms_isReplayGainChecked;
        static constexpr uint32_t kMinCrossFadeLengthInMs = 4000;
        static constexpr uint32_t kMaxCrossFadeLengthInMs = 15000;
        static uint32_t ms_crossFadeLengthInMs;
    };
}
// namespace rePlayer

#include "Player.inl"