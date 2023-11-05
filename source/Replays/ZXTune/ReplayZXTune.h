#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include <binary/container_factories.h>
#include <module/holder.h>

namespace rePlayer
{
    class ReplayZXTune : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::ZXTune>
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
        ~ReplayZXTune() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

        struct Subsong
        {
            std::string path;
            std::string decoderDescription;
            Binary::Data::Ptr data;
            Module::Holder::Ptr holder;
            MediaType type = { eExtension::Unknown, eReplay::ZXTune };
        };

    private:
        ReplayZXTune(Array<Subsong>&& subsongs);

    private:
        Array<Subsong> m_subsongs;
        Module::Renderer::Ptr m_renderer;

        Sound::Chunk m_chunk;
        uint32_t m_availableSamples = 0;
        uint32_t m_loopCount = 0;

        Surround m_surround;
        int32_t m_stereoSeparation;

        uint32_t m_channels = 0;

        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer