#pragma once

#include <Replay.inl.h>

#include <sundog.h>
#include <sunvox_engine.h>

namespace rePlayer
{
    class ReplaySunVox : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        struct Settings : public Command<Settings, eReplay::SunVox>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t extraTime : 16;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplaySunVox() override;

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

    private:
        ReplaySunVox(sunvox_engine* engine);
        void SetPosition(int position);

    private:
        sundog_sound m_sound = {};
        sunvox_engine* m_engine;
        const uint32_t m_numFrames;
        uint32_t m_currentFrame = 0;
        uint32_t m_extraTime = 0;
    };
}
// namespace rePlayer