#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include "AtariAudio/AtariAudio.h"

namespace rePlayer
{
    class ReplaySNDHPlayer : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::SNDHPlayer>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };
            uint32_t durations[0];

            Settings() = default;
            Settings(uint32_t lastSubsong)
            {
                numEntries += uint16_t(lastSubsong + 1);
                for (uint32_t i = 0; i <= lastSubsong; i++)
                    durations[i] = 0;
            }
            uint32_t NumSubsongs() const { return numEntries - 1; }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplaySNDHPlayer() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;
        static constexpr uint32_t kDefaultSongDuration = 180 * 1000; // in milliseconds

    private:
        ReplaySNDHPlayer(SndhFile* sndh, CommandBuffer metadata);

        int32_t GetTickCountFromSc68() const;
        void BuildHash(SndhFile* sndh);
        void BuildDurations(CommandBuffer metadata);

    private:
        SndhFile* m_sndh;

        uint64_t m_currentDuration = 0;
        uint64_t m_currentPosition = 0;
        uint32_t* m_durations;

        Surround m_surround;

        uint32_t m_hash = 0;
        uint32_t m_activeChannels = 0;

    public:
        static int32_t ms_surround;
    };
}
// namespace rePlayer