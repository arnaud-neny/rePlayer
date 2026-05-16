#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

extern "C"
{
#include "libpac/src/pac.h"
#include "libpac/src/pacP.h"
}

namespace rePlayer
{
    class ReplaySBStudio : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::SBStudio>
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
        ~ReplaySBStudio() override;

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
        const Properties& BuildProperties() override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplaySBStudio(pac_module* module);

    private:
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;

        pac_module* m_module;
        Properties m_properties;

    public:
        struct Globals
        {
            int32_t stereoSeparation;
            int32_t surround;
        } static ms_globals;
    };
}
// namespace rePlayer