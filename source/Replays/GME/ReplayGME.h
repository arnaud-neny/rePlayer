#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

typedef struct Music_Emu Music_Emu;

namespace rePlayer
{
    class ReplayGME : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::GME>
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
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayGME() override;

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

    private:
        ReplayGME(Music_Emu* emu, uint16_t* tracks, uint16_t numTracks, CommandBuffer metadata, const char* ext);

        void SetupMetadata(CommandBuffer metadata);

    private:
        Music_Emu* m_emu;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        uint32_t* m_durations = nullptr;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        uint64_t m_currentLoopDuration = 0;
        uint16_t* m_tracks = nullptr;
        uint16_t m_numTracks;
        static int32_t ms_stereoSeparation;
        static int32_t ms_surround;
    };
}
// namespace rePlayer