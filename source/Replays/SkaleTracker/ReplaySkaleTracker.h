#pragma once

#include <Replay.inl.h>
#include <Audio/AudioTypes.h>

#include "Bridge/BridgeShared.h"

namespace rePlayer
{
    class ReplaySkaleTracker : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        struct Settings : public Command<Settings, eReplay::SkaleTracker>
        {
            LoopInfo loop = {};

            Settings() = default;

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplaySkaleTracker() override;

        uint32_t GetSampleRate() const override { return 44100; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        ReplaySkaleTracker(void* hProcess, void* hThread, void* hFileMapping, void* hRequestEvent, void* hResponseEvent, SharedMemory* sharedMemory);

    private:
        void* m_hProcess;
        void* m_hThread;
        void* m_hFileMapping;
        void* m_hRequestEvent;
        void* m_hResponseEvent;
        SharedMemory* m_sharedMemory;

        uint32_t mNumSamples = 0;

        LoopInfo m_loop = {};
        uint64_t m_currentDuration = 0;
        uint64_t m_currentPosition = 0;
    };
}
// namespace rePlayer