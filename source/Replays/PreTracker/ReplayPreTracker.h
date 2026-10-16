#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

extern "C" {
#   include "pretracker/pretracker.h"
}

namespace rePlayer
{
    class ReplayPreTracker : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::PreTracker>
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
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayPreTracker() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

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
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplayPreTracker(PreSong* song1, PreSong* song2, const char version);

    private:
        Surround m_surround;
        std::string m_type;

        PreSong* m_songs[2];

        Properties m_properties;

        uint32_t m_stereoSeparation = 100;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_filter;
        static int32_t ms_patterns;
    };
}
// namespace rePlayer