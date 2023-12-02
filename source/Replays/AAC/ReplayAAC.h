#pragma once

#include <Replay.h>

#include <neaacdec.h>
#include <atomic>

namespace rePlayer
{
    class ReplayAAC : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayAAC() override;

        uint32_t GetSampleRate() const override { return m_samplerate; }
        bool IsSeekable() const override;

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

        bool CanLoop() const override { return false; }

    private:
        struct BitRateData
        {
            uint64_t numFrames = 0;
            uint64_t numBits = 0;
            BitRateData* next = nullptr;
        };

    private:
        ReplayAAC(io::Stream* stream);

        bool Init();
        void AdvanceBuffer(uint32_t bytes);
        void FillBuffer();
        void LookForHeader();

    private:
        SmartPtr<io::Stream> m_stream;

        NeAACDecHandle m_hDecoder = nullptr;

        bool m_isSeekable = true;
        bool m_isEof = false;

        uint8_t m_buffer[FAAD_MIN_STREAMSIZE * 6];
        uint32_t m_bytesIntoBuffer;
        uint32_t m_bytesConsumed;

        uint32_t m_samplerate;
        uint32_t m_position = 0;

        StereoSample* m_sampleBuffer;
        uint32_t m_remainingSamples = 0;
        uint32_t m_numSamples = 0;

        struct
        {
            BitRateData* tail = new BitRateData();
            BitRateData* head = tail;
            BitRateData* free = nullptr;
            uint64_t numFrames = 0;
            uint64_t numBits = 0;
            std::atomic<uint32_t> average = 0;
        } m_bitRate;
    };
}
// namespace rePlayer