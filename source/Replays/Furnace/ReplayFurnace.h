#pragma once

#include <Replay.inl.h>
#include <Audio/AudioTypes.h>

#include "furnace/src/engine/engine.h"

namespace rePlayer
{
    class ReplayFurnace : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayFurnace() override;

        uint32_t GetSampleRate() const override;

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

        static constexpr uint32_t kMaxSamples = 2048;

    private:
        ReplayFurnace(DivEngine* engine);

    private:
        DivEngine* m_engine;
        float m_samples[2][kMaxSamples];
        uint64_t m_position = 0;
        int32_t m_lastLoopPos = -1;
        uint32_t m_numRemainingSamples = 0;
    };
}
// namespace rePlayer