#pragma once

#include <Replay.inl.h>

class V2MPlayer;

namespace rePlayer
{
    class ReplayV2 : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::V2>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideCoreSynth : 1;
                    uint32_t coreSynth : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayV2() override;

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

    private:
        static constexpr uint32_t kSampleRate = 44100; // "The scene is dead" by Dubmood is not playing properly is it's set at 48000Hz

    private:
        ReplayV2(V2MPlayer* player, uint8_t* tune);

    private:
        V2MPlayer* m_player;
        uint8_t* m_tune;
        bool m_hasEnded = false;
        static bool ms_isCoreSynth;
    };
}
// namespace rePlayer