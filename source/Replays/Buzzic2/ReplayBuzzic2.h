#pragma once

#include <Replay.inl.h>
#include "src/Buzzic2.h"

namespace rePlayer
{
    class ReplayBuzzic2 : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayBuzzic2() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 44100;

    private:
        ReplayBuzzic2(Buzzic2* buzzic2);

    private:
        Buzzic2* m_buzzic2;
    };
}
// namespace rePlayer