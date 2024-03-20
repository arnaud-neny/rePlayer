#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

class ReSIDfpBuilder;
class SidDatabase;
class sidplayfp;
class SidTune;

namespace rePlayer
{
    class ReplaySidPlay : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::SidPlay>
        {
            union
            {
                uint32_t value[2] = { 0 };
                struct
                {
                    uint32_t overrideEnableFilter : 1;
                    uint32_t filterEnabled : 1;
                    uint32_t overrideFilter6581 : 1;
                    uint32_t filter6581 : 7;
                    uint32_t overrideFilter8580 : 1;
                    uint32_t filter8580 : 7;
                    uint32_t overrideSidModel : 1;
                    uint32_t sidModel : 1;
                    uint32_t overrideClock : 1;
                    uint32_t clock : 1;
                    uint32_t overrideResampling : 1;
                    uint32_t resampling : 1;
                    uint32_t numSongs : 8;

                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                    uint32_t overridePowerOnDelay : 1;
                    uint32_t powerOnDelay : 13;
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplaySidPlay() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

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
        static constexpr uint32_t kNumSamples = 4096;

        static constexpr uint32_t kDefaultSongDuration = 150 * 1000;

    private:
        ReplaySidPlay(SidTune* sidTune1, SidTune* sidTune2, CommandBuffer metadata);

        uint32_t GetMaxBootingSamples() const;

        static eExtension GetExtension(SidTune* sidTune);
        void SetupMetadata(CommandBuffer metadata);

    private:
        ReSIDfpBuilder* m_residfpBuilder[2] = { nullptr };
        sidplayfp* m_sidplayfp[2] = { nullptr };
        SidTune* m_sidTune[2] = { nullptr };
        Surround m_surround;
        bool m_isSidModelForced : 1 = false;
        bool m_isSidModel8580 : 1 = ms_isSidModel8580;
        bool m_isClockForced : 1 = false;
        bool m_isNtsc : 1 = ms_isNtsc;
        bool m_isResampling : 1 = ms_isResampling;
        uint16_t m_powerOnDelay = uint16_t(ms_powerOnDelay);
        uint32_t* m_durations = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        uint32_t m_numBootSamples = 0;

        int16_t m_samples[kNumSamples * 2];
        uint32_t m_numSamples = 0;

        static uint8_t ms_c64RomKernal[];
        static uint8_t ms_c64RomBasic[];
        static SidDatabase* ms_sidDatabase;

        static bool ms_isFilterEnabled;
        static int32_t ms_filter6581;
        static int32_t ms_filter8580;
        static bool ms_isSidModel8580;
        static bool ms_isNtsc;
        static bool ms_isResampling;
        static int32_t ms_surround;
        static int32_t ms_powerOnDelay;
    };
}
// namespace rePlayer