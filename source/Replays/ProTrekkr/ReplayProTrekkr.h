#pragma once

#include <Replay.h>

namespace rePlayer
{
    class ReplayProTrekkr : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayProTrekkr() override;

        uint32_t GetSampleRate() const override;

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        ReplayProTrekkr(io::Stream* stream);

    private:
        SmartPtr<io::Stream> m_stream;
        uint32_t m_duration;
    };
}
// namespace rePlayer