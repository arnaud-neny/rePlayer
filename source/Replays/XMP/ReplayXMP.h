#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include "libxmp/include/xmp.h"

namespace rePlayer
{
    class ReplayXMP : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::XMP>
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
                    uint32_t overrideA500 : 1;
                    uint32_t a500 : 1;
                    uint32_t overrideInterpolation : 1;
                    uint32_t interpolation : 2;
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 1;
                    uint32_t overrideVblank : 1;
                    uint32_t vblank : 1;
                    uint32_t overrideFx9bug : 1;
                    uint32_t fx9bug : 1;
                    uint32_t overrideFixLoop : 1;
                    uint32_t fixLoop : 1;
                    uint32_t overrideMode : 1;
                    uint32_t mode : 4;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayXMP() override;

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
        ReplayXMP(xmp_context contextPlayback, xmp_context contextVisuals);

        void SetStates(xmp_context context);

    private:
        xmp_context m_contextPlayback;
        xmp_context m_contextVisuals;
        xmp_module_info m_moduleInfo;

        xmp_frame_info m_frameInfo = {};
        uint32_t m_remainingSamples = 0;
        bool m_hasLooped = false;

        union States
        {
            uint16_t value = 0;
            struct
            {
                uint32_t a500 : 1;
                uint32_t interpolation : 2;
                uint32_t filter : 1;
                uint32_t vblank : 1;
                uint32_t fx9bug : 1;
                uint32_t fixLoop : 1;
                uint32_t mode : 4;
            };
        } m_states;

        Surround m_surround;
        uint32_t m_stereoSeparation = 100;

        uint32_t m_visualsSamples = 0;

        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_a500;
        static int32_t ms_interpolation;
        static int32_t ms_filter;
        static int32_t ms_vblank;
        static int32_t ms_fx9bug;
        static int32_t ms_fixLoop;
        static int32_t ms_mode;
        static int32_t ms_patterns;
    };
}
// namespace rePlayer