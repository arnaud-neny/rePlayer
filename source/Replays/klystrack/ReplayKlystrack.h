#pragma once

#include <Replay.inl.h>
#include <Audio/AudioTypes.h>

#include "lib/ksnd.h"
extern "C"
{
#include "snd/music.h"
}

namespace rePlayer
{
    class ReplayKlystrack : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayKlystrack() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;
        const Properties& BuildProperties() override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplayKlystrack(KPlayer* player, KSong* song);

    private:
        KPlayer* m_player;
        KSong* m_song;
        KSongInfo m_info;
        bool m_hasLooped = false;
        Properties m_properties;
    };
}
// namespace rePlayer