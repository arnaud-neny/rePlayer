#pragma once

#include <Replay.inl.h>

#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>

namespace rePlayer
{
    class ReplayPsycle : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        struct Settings : public Command<Settings, eReplay::Psycle>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t loadNewBlitz : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayPsycle() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 44100;

    private:
        ReplayPsycle(bool isPsycle3);

    private:
        psycle::host::Song* m_song;
        std::vector<char*> m_clipboardMem;
        uint32_t m_clipboarMemSize = 0;
        bool m_isPsycle3;
    };
}
// namespace rePlayer