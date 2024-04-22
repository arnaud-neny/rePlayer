#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>
#include <Containers/Array.h>
extern "C"
{
#include "mdxmini/src/mdxmini.h"
}

namespace rePlayer
{
    class ReplayMDX : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::MDX>
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
                    uint32_t overrideLoops : 1;
                    uint32_t loops : 4;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayMDX() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

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

    private:
        ReplayMDX(t_mdxmini&& mdx, uint32_t fileIndex, io::Stream* stream, SmartPtr<io::Stream> mdxStream);

        static size_t LoadCB(const char* filename, void** buf, void* user_data);

    private:
        SmartPtr<io::Stream> m_stream;
        Array<uint32_t> m_subsongs;
        t_mdxmini m_mdx;
        std::string m_filename;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        uint32_t m_loops = uint32_t(ms_loops);
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_loops;
    };
}
// namespace rePlayer