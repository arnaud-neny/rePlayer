#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include "emulation/player.h"
#include "track/track.h"

namespace rePlayer
{
    class ReplayTIATracker : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::TIATracker>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t overrideSurround : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayTIATracker() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 44100;

    private:
        ReplayTIATracker(Track::Track&& track);

    private:
        Track::Track m_track;
        Emulation::Player m_player;
        uint32_t m_numRemainingSamples = 0;
        const uint32_t m_numSamplesPerFrame;
        int16_t* m_samples;
        Surround m_surround;
        int32_t m_stereoSeparation;
        int32_t m_trackCurNoteIndex[2] = { INT_MAX, INT_MAX };
        bool m_isLoopîng = false;
        struct Order
        {
            int v[2];
            bool operator==(const Order& other) const { return v[0] == other.v[0] && v[1] == other.v[1]; }
        };
        Array<Order> m_sequences;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer