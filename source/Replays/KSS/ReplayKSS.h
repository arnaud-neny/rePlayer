#pragma once

#include <Replay.inl.h>

#include <kssplay.h>

namespace rePlayer
{
    class ReplayKSS : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::KSS>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t numSongsMinusOne : 8;
                    uint32_t overrideMasterVolume : 1;
                    uint32_t masterVolume : 8;
                    uint32_t overrideDevicePan : 1;
                    uint32_t devicePan : 8;
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayKSS() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;
        static constexpr uint32_t kDefaultSongDuration = 150 * 1000;

    private:
        ReplayKSS(KSS* kss, CommandBuffer metadata);
        ReplayKSS(io::Stream* stream, Array<uint32_t>&& subsongs, CommandBuffer metadata);

        void SetupMetadata(CommandBuffer metadata);

    private:
        KSS* m_kss = nullptr;
        KSSPLAY* m_kssplay = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;

        uint32_t m_durations[256] = { 0 };

        int32_t m_loopCount = 0;

        SmartPtr<io::Stream> m_stream;
        Array<uint32_t> m_subsongs;

        std::string m_title;
        uint32_t m_currentSubsongIndex = 0xffFFffFF;

        static int32_t ms_masterVolume;
        static int32_t ms_devicePan;
        static int32_t ms_silence;
    };
}
// namespace rePlayer