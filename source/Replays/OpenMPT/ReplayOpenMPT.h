#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

struct openmpt_module;

namespace rePlayer
{
    class ReplayOpenMPT : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::OpenMPT>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t overrideInterpolation : 1;
                    uint32_t interpolation : 3;
                    uint32_t overrideRamping : 1;
                    uint32_t ramping : 4;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                    uint32_t overrideVblank : 1;
                    uint32_t vblank : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayOpenMPT() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

        struct GlobalSettings
        {
            const char* serializedName;
            const char* format;
            int32_t isEnabled = 0;
            int32_t interpolation = 0;
            int32_t ramping = -1;
            int32_t stereoSeparation = 100;
            int32_t surround = 0;
            int32_t vblank = -1;

            std::string labels[6];

            GlobalSettings(const char* name, const char* fmt, bool isSurroundEnabled = false, bool isEnbl = false) : serializedName(name), format(fmt), surround(isSurroundEnabled), isEnabled(isEnbl) {}
     };

    private:
        ReplayOpenMPT(openmpt_module* module);

        static size_t OnRead(void* stream, void* dst, size_t bytes);
        static int32_t OnSeek(void* stream, int64_t offset, int32_t whence);
        static int64_t OnTell(void* stream);

        static MediaType BuildMediaType(openmpt_module* module);

    private:
        openmpt_module* m_module = nullptr;
        uint64_t m_silenceStart : 63 = 0;
        uint64_t m_isSilenceTriggered : 1 = 0;
        uint64_t m_currentPosition = 0;
        uint32_t m_previousSubsongIndex = 0xffFFffFF;
        Surround m_surround;

        static GlobalSettings ms_settings[];
    };
}
// namespace rePlayer