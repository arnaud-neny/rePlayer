#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include <uade/uade.h>

#include <filesystem>

namespace rePlayer
{
    class ReplayUADE : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        static Array<std::pair<std::string, std::string>> GetFileFilters();

        struct Settings : public Command<Settings, eReplay::UADE>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t unused1 : 8;
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                    uint32_t unused2 : 2;
                    uint32_t overrideNtsc : 1;
                    uint32_t ntsc : 1;
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 2;
                    uint32_t overridePlayer : 1;
                };
            };
            uint32_t data[0];

            Settings() = default;
            Settings(uint32_t numSubsongs)
            {
                numEntries += uint8_t(numSubsongs) * 2;
                for (uint32_t i = 0; i < numSubsongs * 2; i++)
                    data[i] = 0;
            }
            uint32_t NumSubsongs() const { return (numEntries - 1 - overridePlayer) / 2; }

            LoopInfo* GetLoops() { return reinterpret_cast<LoopInfo*>(data + overridePlayer); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayUADE() override;

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
        ReplayUADE(io::Stream* stream, uade_state* uadeState, ReplayOverride* replayOverride);

        static struct uade_file* AmigaLoader(const char* name, const char* playerdir, void* context, struct uade_state* state);
        static void LoadPlayerNames();

        void BuildMediaType();
        void BuildDurations(CommandBuffer metadata);

    private:
        uint32_t m_lastSubsongIndex = 0;
        SmartPtr<io::Stream> m_stream;
        SmartPtr<io::Stream> m_tempStream;
        uade_state* m_uadeState;
        LoopInfo* m_loops = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        ReplayOverride* m_replayOverride = nullptr;
        Array<std::string> m_packagedSubsongNames;
        static ReplayOverride ms_replayOverrides[];

    public:
        struct Globals
        {
            Array<char> strings;
            Array<std::pair<uint32_t, uint32_t>> players;

            std::string dataPath;

            int32_t stereoSeparation;
            int32_t surround;
            int32_t filter;
            int32_t isNtsc;
        } static ms_globals;
    };
}
// namespace rePlayer