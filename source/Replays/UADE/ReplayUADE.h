#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include <uade/uade.h>

#include <filesystem>

namespace rePlayer
{
    struct DllEntry;

    class ReplayUADE : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        static std::string GetFileFilters();

        struct Settings : public Command<Settings, eReplay::UADE>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t numSongs : 8;
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                    uint32_t _deprecated : 2;
                    uint32_t overrideNtsc : 1;
                    uint32_t ntsc : 1;
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 2;
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayUADE() override;

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
        static constexpr uint32_t kDefaultSongDuration = 60 * 10 * 1000;

        struct ReplayOverride
        {
            const char* header;
            eExtension (*build)(ReplayUADE*, const uint8_t*&, std::filesystem::path&, size_t&);
            bool (*load)(ReplayUADE*, const uint8_t*&, const char*, size_t&);
            void (*main)(ReplayUADE*, const uint8_t*&, size_t&) = [](ReplayUADE* replay, const uint8_t*& data, size_t& dataSize)
            {
                (void)replay;
                dataSize = *reinterpret_cast<const uint32_t*>(data + 8) - 12;
                data += 12;
            };
        };

    private:
        ReplayUADE(io::Stream* stream, uade_state* uadeState);

        static struct uade_file* AmigaLoader(const char* name, const char* playerdir, void* context, struct uade_state* state);

        void BuildMediaType();
        virtual void SetSettings(int32_t stereoSeparation, int32_t surround, int32_t filter, int32_t isNtsc);
        void SetupMetadata(CommandBuffer metadata);

    private:
        uint32_t m_lastSubsongIndex = 0;
        SmartPtr<io::Stream> m_stream;
        SmartPtr<io::Stream> m_tempStream;
        uade_state* m_uadeState;
        uint32_t* m_durations = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        size_t m_dllIndex;
        Array<DllEntry>* m_dllEntries = nullptr;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_filter;
        static int32_t ms_isNtsc;
        static ReplayOverride ms_replayOverrides[];
    };
}
// namespace rePlayer