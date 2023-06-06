#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>
#include <player/playera.hpp>

namespace rePlayer
{
    class ReplayVGM : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::VGM>
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
                    uint32_t overrideDroV2Opl3 : 1;
                    uint32_t droV2Opl3 : 2;
                    uint32_t overrideVgmPlaybackHz : 1;
                    uint32_t vgmPlaybackHz : 2;
                    uint32_t overrideVgmHardStopOld : 1;
                    uint32_t vgmHardStopOld : 1;
                };
            };
            uint32_t duration = 0;

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayVGM() override;

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
        static constexpr uint32_t kSampleBufferLen = 2048;
        static constexpr uint32_t kDefaultSongDuration = 180 * 1000;

    private:
        ReplayVGM(io::Stream* stream, PlayerA* player, DATA_LOADER* loader);

        static UINT8 OnEvent(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam);

        static eExtension GetExtension(io::Stream* stream, PlayerA* player, DATA_LOADER* loader);

    private:
        SmartPtr<io::Stream> m_stream;
        PlayerA* m_player;
        DATA_LOADER* m_loader;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        bool m_hasEnded = false;
        bool m_hasLooped = false;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_droV2Opl3;
        static int32_t ms_vgmPlaybackHz;
        static int32_t ms_vgmHardStopOld;
    };
}
// namespace rePlayer