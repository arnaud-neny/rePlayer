#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>
#include <Containers/SmartPtr.h>

namespace rePlayer
{
    enum class CoreSwan : int32_t
    {
        kMednafen,
        kOswan,
        kInWSR
    };

    class ReplayWonderSwan : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::WonderSwan>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                    uint32_t overrideCore : 1;
                    uint32_t core : 2;
                };
            };
            uint32_t subsongs[8] = { 0 }; // 256 / sizeof(uint32_t)
            LoopInfo loops[0];

            Settings() = default;
            Settings(uint32_t lastSubsong)
            {
                numEntries += uint16_t(lastSubsong + 1) * 2;
                for (uint32_t i = 0; i <= lastSubsong; i++)
                    loops[i] = {};
            }
            uint32_t NumSubsongs() const { return (numEntries - 1 - 8) / 2; }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayWonderSwan() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

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
        static constexpr uint32_t kNumSamples = 640;

    private:
        ReplayWonderSwan(CoreSwan core, CommandBuffer metadata, Array<std::string>&& titles);

        void BuildDurations(CommandBuffer metadata);

    private:
        SmartPtr<io::Stream> m_stream;
        int16_t m_samples[kNumSamples * 2];
        uint32_t m_remainingSamples = 0;
        CoreSwan m_core = CoreSwan::kMednafen;

        uint64_t m_currentDuration = 0;
        uint64_t m_currentPosition = 0;
        uint64_t m_globalPosition = 0;

        Surround m_surround;

        LoopInfo m_loops[256] = {};
        uint8_t m_subsongs[256] = {};
        uint32_t m_numSubsongs = 256;

        Array<std::string> m_titles;

    public:
        struct Globals
        {
            int32_t surround;
            CoreSwan core;
        } static ms_globals;
    };
}
// namespace rePlayer