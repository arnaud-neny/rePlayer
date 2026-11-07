#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include <nuade/nuade.h>

namespace rePlayer
{
    class ReplayNuade : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::Nuade>
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
                    uint32_t overridePanning : 1;
                    uint32_t panning : 1;
                    uint32_t overridePlayer : 1;
                };
            };
            uint32_t data[0];

            Settings() = default;
            Settings(uint32_t numSubsongs)
            {
                numEntries += uint8_t(numSubsongs) * 2;
                for (uint32_t i = 0; i < numSubsongs * 2 + 1; i++)
                    data[i] = 0;
            }
            uint32_t NumSubsongs() const { return (numEntries - 1 - overridePlayer) / 2; }

            LoopInfo* GetLoops() { return reinterpret_cast<LoopInfo*>(data + overridePlayer); }


            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayNuade() override;

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
        const Properties& BuildProperties() override;

        Patterns UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags) override;

        static void BuildPlayerNames();
        void BuildDurations(CommandBuffer metadata);

    private:
        static constexpr uint32_t kSampleRate = 48000;
        static constexpr uint32_t kDefaultSongDuration = 60 * 10 * 1000;

        struct Loader
        {
            static bool exists(Loader* self, const char* url);
            static NuadeIoReadResult read_url_to_memory(Loader* self, const char* url);
            static void free_url_to_memory(Loader* self, void* memory);

            SmartPtr<io::Stream> stream;
            Array<std::string> subsongNames;
        };

    private:
        ReplayNuade(NuadeSong* song1, NuadeSong* song2, const char* extension, Loader&& loader);

        void PreparePatterns();

    private:
        Surround m_surround;

        NuadeSong* m_songs[2];
        LoopInfo* m_loops = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        float* m_panning;

        Loader m_loader;
        uint32_t m_lastSubsongIndex = 0;

        Properties m_properties;
        struct
        {
            NuadeTrackerInfo trackerInfo;
            uint8_t noteWidth;
            uint8_t instWidth;
            uint8_t numEffects;
            uint8_t fxChars;
            int8_t fxColumn;
            int8_t argColumn;
        } m_patterns;

        bool m_hasLooped = false;
        uint32_t m_stereoSeparation = 100;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
        static int32_t ms_panning;
        static int32_t ms_patterns;
        static Array<std::pair<uint32_t, uint32_t>> ms_players;
        static std::string ms_uadePath;
    };
}
// namespace rePlayer