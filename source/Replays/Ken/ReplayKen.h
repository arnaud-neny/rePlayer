#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

namespace rePlayer
{
    class ReplayKen : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::Ken>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayKen() override;

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
        static constexpr uint32_t kSampleSize = 576;
    private:
        ReplayKen(eExtension ext, uint32_t duration);

    private:
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        int16_t m_samples[kSampleSize * 2 * 2] = {};
        uint32_t m_remainingSamples = 0;
        uint32_t m_numLoops = 0;
        bool m_hasLooped = false;

        uint32_t m_duration;

    public:
        struct Globals
        {
            int32_t stereoSeparation;
            int32_t surround;
        } static ms_globals;
    };
}
// namespace rePlayer