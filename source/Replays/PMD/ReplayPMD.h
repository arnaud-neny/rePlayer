#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>
#include <Containers/Array.h>

#define PMD_REPLAYER
#include <pmdwincore.h>

namespace rePlayer
{
    class ReplayPMD : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::PMD>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayPMD() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

        uint32_t Render(core::StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 44100;

    private:
        ReplayPMD(PMDWIN* pmd, std::string&& info);

    private:
        Surround m_surround;

        PMDWIN* m_pmd;
        std::string m_info;

        uint32_t m_stereoSeparation = 100;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer