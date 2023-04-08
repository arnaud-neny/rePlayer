#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

namespace SoundMon
{
    class Module;
    class Player;
}
//namespace SoundMon

namespace rePlayer
{
    class ReplaySoundMon : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::SoundMon>
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
        ~ReplaySoundMon() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
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
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplaySoundMon(SoundMon::Module* module);

    private:
        SoundMon::Module* m_module = nullptr;
        SoundMon::Player* m_player = nullptr;
        Surround m_surround;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer