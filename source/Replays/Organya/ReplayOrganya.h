#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include "Organya/OrganyaDecoder.h"

namespace rePlayer
{
    class ReplayOrganya : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayOrganya() override;

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
        ReplayOrganya(Organya::Song* song);

    private:
        Organya::Song* m_song;

        std::vector<float> m_samples;
        uint32_t m_remainingSamples = 0;
        bool m_hasLooped = false;
        uint64_t m_currentPosition = 0;
    };
}
// namespace rePlayer