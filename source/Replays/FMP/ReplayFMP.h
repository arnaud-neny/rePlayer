#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>
#include <Containers/Array.h>

extern "C"
{
#   include <common/fmplayer_common.h>
#   include <common/fmplayer_file.h>
#   include <fmdriver/fmdriver.h>
}
#include <libopna/opna.h>
#include <libopna/opnatimer.h>

namespace rePlayer
{
    class ReplayFMP : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::FMP>
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
        ~ReplayFMP() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

        uint32_t Render(core::StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 55467;

    private:
        ReplayFMP(io::Stream* stream, Array<uint32_t>&& subsongs, eExtension ext);

    private:
        Surround m_surround;
        SmartPtr<io::Stream> m_stream;
        Array<uint32_t> m_subsongs;
        struct FMP
        {
            struct opna opna;
            struct opna_timer timer;
            struct fmdriver_work work;
            struct fmplayer_file* fmfile;
            struct ppz8 ppz8;
            char adpcmram[OPNA_ADPCM_RAM_SIZE];
        } m_fmp;
        std::string m_info;
        std::string m_title;
        uint8_t m_oldLoopCnt;

        uint32_t m_stereoSeparation = 100;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer