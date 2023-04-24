#pragma once

#include <Replay.inl.h>

#include <nezplug/nezplug.h>

namespace rePlayer
{
    class ReplayNEZplug : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::NEZplug>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t numSongsMinusOne : 8;
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 2;
                    uint32_t overrideGain : 1;
                    uint32_t gain : 8;
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayNEZplug() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;
        static constexpr uint32_t kDefaultSongDuration = 150 * 1000;

    private:
        ReplayNEZplug(NEZ_PLAY* nezPlay, eExtension extension, CommandBuffer metadata);

        void SetupMetadata(CommandBuffer metadata);

    private:
        NEZ_PLAY* m_nezPlay;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;

        uint32_t m_durations[256] = { 0 };

        static int32_t ms_gain;
        static int32_t ms_filter;
    };
}
// namespace rePlayer