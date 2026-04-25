#pragma once

#include <Replay.inl.h>

struct COPLprops;
class Copl;
class CPlayer;

namespace rePlayer
{
    class ReplayAdLib : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        static Array<std::pair<std::string, std::string>> GetFileFilters();

    public:
        ~ReplayAdLib() override;

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
        const Properties& BuildProperties() override;

        Patterns UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags) override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

        enum class Opl : int32_t
        {
            kDOSBoxOPL3Emulator,
            kNukedOPL3Emulator,
            kKenSilvermanEmulator,
            kMameEmulator
        };

    private:
        ReplayAdLib(const std::string& filename, Copl* opl, CPlayer* player, Copl* silentOpl, CPlayer* silentPlayer);

        static COPLprops CreateCore(bool isStereo);
        static const char* GetExtension(const std::string& filename, CPlayer* player);

    private:
        std::string m_filename;
        Copl* m_opl;
        CPlayer* m_player;
        uint32_t m_remainingSamples = 0;
        bool m_hasEnded = false;
        uint8_t m_isStuck = 0;

        struct
        {
            Copl* opl;
            CPlayer* player;
            uint32_t remainingSamples = 0;
            uint8_t isStuck = 0;
            Properties properties;
        } m_silent;

        static int32_t ms_surround;
        static Opl ms_opl;
    };
}
// namespace rePlayer