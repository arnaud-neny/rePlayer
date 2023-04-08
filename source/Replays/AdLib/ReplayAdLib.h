#pragma once

#include <Replay.h>

struct COPLprops;
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

        static std::string GetFileFilters();

    public:
        ~ReplayAdLib() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

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
        ReplayAdLib(const std::string& filename, COPLprops* core, CPlayer* player);

        static COPLprops* CreateCore(bool isStereo);
        static const char* GetExtension(const std::string& filename, CPlayer* player);

    private:
        std::string m_filename;
        struct COPLprops* m_core;
        CPlayer* m_player;
        uint32_t m_remainingSamples = 0;
        bool m_hasEnded = false;
        uint8_t m_isStuck = 0;

        static int32_t ms_surround;
        static Opl ms_opl;
    };
}
// namespace rePlayer