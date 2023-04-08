#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

struct ASAP;

namespace rePlayer
{
    class ReplayASAP : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::ASAP>
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
        ~ReplayASAP() override;

        uint32_t GetSampleRate() const override;
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        ReplayASAP(ASAP* song);

    private:
        ASAP* m_song;
        uint64_t m_position = 0;
        uint64_t m_duration = 0;
        Surround m_surround;
        uint32_t m_stereoSeparation = 1000;
        bool m_hasEnded = false;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer