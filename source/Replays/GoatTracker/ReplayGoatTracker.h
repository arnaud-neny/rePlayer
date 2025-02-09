#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

namespace rePlayer
{
    class ReplayGoatTracker : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::GoatTracker>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideSidModel : 1;
                    uint32_t isSidModel8580 : 1;
                    uint32_t overrideNtsc : 1;
                    uint32_t isNtsc : 1;
                    uint32_t overrideFiltering : 1;
                    uint32_t isFiltering : 1;
                    uint32_t overrideDistortion : 1;
                    uint32_t isDistortion : 1;

                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayGoatTracker() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
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
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplayGoatTracker(uint32_t numSongs, uint32_t songId, bool isNtsc);

    private:
        Surround m_surround;
        const uint32_t m_numSongs;
        const uint32_t m_songId;

        const uint32_t m_tempo;
        uint32_t m_remainingSamples = 0;
        bool m_hasLooped = false;

        Array<struct Sequence> m_sequence;

    public:
        struct Globals
        {
            bool isSidModel8580;
            bool isNtsc;
            bool isFiltering;
            bool isDistortion;
            int32_t surround;
        } static ms_globals;
    };
}
// namespace rePlayer