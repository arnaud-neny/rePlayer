#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>
#include <Containers/SmartPtr.h>

#define DRWAV_MALLOC(sz) core::Alloc((sz))
#define DRWAV_REALLOC(p, sz) core::Realloc((p), (sz))
#define DRWAV_FREE(p) core::Free((p))
#define DR_WAV_NO_STDIO
#include <dr_wav.h>

namespace rePlayer
{
    class ReplayWAV : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayWAV() override;

        uint32_t GetSampleRate() const override { return m_wav->sampleRate; }
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

        bool CanLoop() const override { return false; }

    private:
        ReplayWAV(io::Stream* stream, drwav* wav);

        static size_t OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead);
        static drwav_bool32 OnSeek(void* pUserData, int offset, drwav_seek_origin origin);
        static drwav_bool32 OnTell(void* pUserData, drwav_int64* pCursor);

    private:
        SmartPtr<io::Stream> m_stream;
        drwav* m_wav;
    };
}
// namespace rePlayer