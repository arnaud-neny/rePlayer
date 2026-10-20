#pragma once

#include <Replay.inl.h>

#include "v2redux/v2redux.h"
#include "v2m_pattern.h"

namespace rePlayer
{
    class ReplayV2 : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::V2>
        {
            union
            {
                uint32_t value[4] = { 0, 0 };
                struct
                {
                    uint32_t version : 3;
                    uint32_t overriddenDeltas : 29;
                    uint32_t deltas : 29;
                    uint32_t seed[2];
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayV2() override;

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

        Patterns UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags) override;

    private:
        static constexpr uint32_t kSampleRate = 44100;

    private:
        ReplayV2();

    private:
        v2redux::Player m_player;
        v2mpat::Pattern m_pattern;
        bool m_hasEnded = false;

        uint32_t m_numPatSamples = 0;

        static int32_t ms_patterns;
    };
}
// namespace rePlayer