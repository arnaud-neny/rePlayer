#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>
// #include <Containers/Array.h>
#include <Containers/SmartPtr.h>

#include "libdsd2pcm/dsd_pcm_converter_hq.h"
#include "libdstdec/dst_decoder.h"

namespace rePlayer
{
    class ReplayDSD : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayDSD() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return !m_arrDsdBuf || !m_dff.isDstEncoded; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

        bool CanLoop() const override { return false; }

    private:
        static constexpr uint32_t kSampleRate = 192000; // or 96000

        struct DSF
        {
            uint8_t* blockData = nullptr;
            uint64_t fileSize;
            uint64_t id3Offset;
            uint64_t numSamples;
            uint64_t dataOffset;
            uint64_t dataSize;
            uint64_t dataEnd;
            uint32_t numChannels;
            uint32_t sampleRate;
            uint32_t blockSize;
            uint32_t blockOffset;
            uint32_t blockEnd;
            bool isLSB;
            uint8_t swapBits[256];
        };

        struct DFF
        {
            uint64_t dstiOffset;
            uint64_t dstiSize;
            uint64_t dataOffset;
            uint64_t dataSize;
            uint64_t currentOffset;
            uint64_t currentSize;
            uint32_t version;
            uint32_t sampleRate;
            uint32_t frameSize;
            uint32_t numFrames;
            uint16_t numChannels;
            uint16_t frameRate;
            uint16_t loudspeakerConfig;
            bool isDstEncoded;

            struct Subsong
            {
                uint64_t start;
                uint64_t stop;
            };
//             Array<Subsong> subsongs;
        };

    private:
        ReplayDSD(io::Stream* stream, DSF&& dsf);
        ReplayDSD(io::Stream* stream, DFF&& dff);

        uint32_t ReadDSF();
        std::pair<uint32_t, int32_t> ReadDFF();

    private:
        SmartPtr<io::Stream> m_stream;
        DSF m_dsf;
        DFF m_dff;
        dsdpcm_converter_hq m_converter;
        CDSTDecoder m_decoder;
        uint32_t m_dstSize;
        int m_numPcmOutDelta;
        int m_numPcmOutSamples;
        uint8_t* m_arrDsdBuf;
        uint8_t* m_arrDstBuf;
        float* m_arrPcmBuf;

        uint32_t m_remainingSamples = 0;
        uint32_t m_numSamples = 0;
    };
}
// namespace rePlayer