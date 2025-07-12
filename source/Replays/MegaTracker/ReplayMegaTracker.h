#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

extern "C"
{
#   include "mtpng/mtptypes.h"
}

namespace rePlayer
{
    class ReplayMegaTracker : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::MegaTracker>
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
                    uint32_t overrideOldStyle : 1;
                    uint32_t oldStyle : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayMegaTracker() override;

        uint32_t GetSampleRate() const override;
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        ReplayMegaTracker(MTPInputT* plugin);

    private:
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        uint32_t m_numSamples = 0;
        bool m_oldStyle = false;

        MTPInputT* m_plugin;

    public:
        struct Globals
        {
            int32_t stereoSeparation;
            int32_t surround;
            int32_t oldStyle;
        } static ms_globals;
    };
}
// namespace rePlayer