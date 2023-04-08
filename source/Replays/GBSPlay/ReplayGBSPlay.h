#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

extern "C"
{
#include "gbsplay/libgbs.h"
}

namespace rePlayer
{
    class ReplayGBSPlay : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::GBSPlay>
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
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 2, : 11;
                    uint32_t numSongs : 8;
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayGBSPlay() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;
        static constexpr uint32_t kDefaultSongDuration = 150000;
        static constexpr uint32_t kMaxSamples = 1024;

    private:
        ReplayGBSPlay(gbs* gbs, CommandBuffer metadata);

        static void OnSound(struct gbs* const gbs, struct gbs_output_buffer* buf, void* priv);

        static eExtension GetExtension(gbs* gbs);
        void SetupMetadata(CommandBuffer metadata);

    private:
        gbs* m_gbs;
        gbs_output_buffer m_buf;
        int16_t m_bufData[kMaxSamples * 2];
        int16_t m_bufDataCopy[kMaxSamples * 2];
        long m_bufPos = kMaxSamples;
        uint32_t* m_durations = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        static int32_t ms_filter;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer