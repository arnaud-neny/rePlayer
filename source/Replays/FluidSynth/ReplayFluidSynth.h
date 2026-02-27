#pragma once

#include <Replay.inl.h>
#include <Containers/Array.h>

#include <fluidsynth.h>

namespace rePlayer
{
    class ReplayFluidSynth : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::FluidSynth>
        {
            union
            {
                uint32_t uGain = 0;
                float gain;
            };
            uint16_t polyphony = 0;
            char soundfont[2] = { 0, 0 };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayFluidSynth() override;

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
        ReplayFluidSynth(fluid_settings_t* settings, fluid_synth_t* synth, fluid_player_t* player);

    private:
        struct
        {
            fluid_settings_t* settings;
            fluid_synth_t* synth;
            fluid_player_t* player;
        } m_fs;

        std::string m_info;

        static float ms_gain;
        static int32_t ms_polyphony;
        static std::string ms_soundfont;
    };
}
// namespace rePlayer