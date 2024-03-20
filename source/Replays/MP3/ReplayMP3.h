#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>

#define DRMP3_MALLOC(sz) core::Alloc((sz))
#define DRMP3_REALLOC(p, sz) core::Realloc((p), (sz))
#define DRMP3_FREE(p) core::Free((p))
#define DR_MP3_NO_STDIO
#define DR_MP3_FLOAT_OUTPUT
#include <dr_mp3.h>

namespace rePlayer
{
    class ReplayMP3 : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayMP3() override;

        uint32_t GetSampleRate() const override { return m_mp3->sampleRate; }
        bool IsSeekable() const override;
        bool IsStreaming() const override;

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetStreamingTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

        bool CanLoop() const override { return false; }

    private:
        struct StreamData
        {
            SmartPtr<io::Stream> stream;
            bool isSeekable;
            bool isInit;
        };

    private:
        ReplayMP3(StreamData* streamData, drmp3* mp3);

        static size_t OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead);
        static drmp3_bool32 OnSeek(void* pUserData, int offset, drmp3_seek_origin origin);

    private:
        StreamData* m_streamData;
        drmp3* m_mp3;
    };
}
// namespace rePlayer