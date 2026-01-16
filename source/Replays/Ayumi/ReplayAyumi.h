#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

extern "C" {
#   include "ayumi/ayumi.h"
#   include "ayumi/load_text.h"
}

namespace rePlayer
{
    class ReplayAyumi : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::Ayumi>
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
            LoopInfo loop = {};

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayAyumi() override;

        uint32_t GetSampleRate() const override { return uint32_t(m_textData.sample_rate); }
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
        ReplayAyumi(SmartPtr<io::Stream> stream, CommandBuffer metadata, const text_data& textData, const ayumi& ay);

    private:
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;

        SmartPtr<io::Stream> m_stream;
        const text_data m_textData;
        ayumi m_ay;

        LoopInfo m_loop = {};
        uint64_t m_currentDuration = 0;
        uint64_t m_currentPosition = 0;
        uint64_t m_globalPosition = 0;

        const double m_isrStep;
        double m_isrCounter = 1.0; // keep overflows between calls

    public:
        struct Globals
        {
            int32_t stereoSeparation;
            int32_t surround;
        } static ms_globals;
    };
}
// namespace rePlayer