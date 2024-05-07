#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

struct hvl_tune;
struct hvl_state;

namespace rePlayer
{
    class ReplayHively : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::Hively>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t overrideSurround : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayHively() override;

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
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplayHively(hvl_tune* modulePlayback, hvl_tune* moduleVisuals, const char* extension);

    private:
        hvl_tune* m_modulePlayback = nullptr;
        hvl_tune* m_moduleVisuals = nullptr;
        int16_t* m_tickData;
        Surround m_surround;
        uint32_t m_numPlaybackAvailableSamples = 0;
        uint32_t m_numVisualsAvailableSamples = 0;
        bool m_arePatternsDisplayed = true;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_patterns;
    };
}
// namespace rePlayer