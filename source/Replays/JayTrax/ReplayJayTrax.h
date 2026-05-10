#pragma once

#include <Replay.inl.h>
#include <Audio/AudioTypes.h>

#include "CrossX/SoundEngine.h"

namespace rePlayer
{
    class ReplayJayTrax : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayJayTrax() override;

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
        const Properties& BuildProperties() override;

        Patterns UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags) override;

    private:
        static constexpr uint32_t kSampleRate = 44100;

    private:
        ReplayJayTrax(SoundEngine* soundEngine, io::Stream* stream);

    private:
        SoundEngine* m_soundEngine;
        SoundEngine m_displayEngine;
        Properties m_properties;
    };
}
// namespace rePlayer