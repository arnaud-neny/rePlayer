#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

namespace rePlayer
{
    class ReplayGoatTracker : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::GoatTracker>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideSidModel : 1;
                    uint32_t isSidModel8580 : 1;
                    uint32_t overrideNtsc : 1;
                    uint32_t isNtsc : 1;
                    uint32_t overrideFiltering : 1;
                    uint32_t isFiltering : 1;
                    uint32_t overrideDistortion : 1;
                    uint32_t isDistortion : 1;
                    uint32_t overrideMultiplier : 1;
                    uint32_t multiplier : 5;
                    uint32_t overrideFineVibrato : 1;
                    uint32_t fineVibrato : 1;
                    uint32_t overridePulseOptimization : 1;
                    uint32_t pulseOptimization : 1;
                    uint32_t overrideEffectOptimization : 1;
                    uint32_t effectOptimization : 1;

                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };
            union
            {
                uint32_t value2 = 0;
                struct
                {
                    uint32_t overrideHardRestart : 1;
                    uint32_t hardRestart : 16;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayGoatTracker() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

        struct Globals
        {
            bool isSidModel8580;
            bool isNtsc;
            bool isFiltering;
            bool isDistortion;
            uint8_t multiplier;
            bool fineVibrato;
            bool pulseOptimization;
            bool effectOptimization;
            uint32_t hardRestart;
            int32_t surround;
        };

    private:
        ReplayGoatTracker(uint32_t numSongs, uint32_t songId, Globals& globals, const std::string& name);

    private:
        Surround m_surround;
        const uint32_t m_numSongs;
        const uint32_t m_songId;
        std::string m_name;

        uint32_t m_tempo;
        uint32_t m_remainingSamples = 0;
        bool m_hasLooped = false;
        bool m_hasFailed = false;
        mutable bool m_hasNoticedFailure = false;

        Array<struct Sequence> m_sequence;

    public:
        static Globals ms_globals;

    private:
        Globals m_currentSettings;
        Globals m_updatedSettings;
    };
}
// namespace rePlayer