#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

class CYmMusic;

namespace rePlayer
{
    class ReplayStSound : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::StSound>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideLowPassFilter : 1;
                    uint32_t lowPassFilter : 1;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayStSound() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override;

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
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplayStSound(CYmMusic* module);

    private:
        CYmMusic* m_module;
        uint32_t m_loop = 0;
        static bool ms_isLowpassFilterEnabled;
        Surround m_surround;
        static int32_t ms_surround;
    };
}
// namespace rePlayer