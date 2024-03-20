#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>
#include <Containers/SmartPtr.h>

#include <wavpack.h>

namespace rePlayer
{
    class ReplayWavPack : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayWavPack() override;

        uint32_t GetSampleRate() const override;
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
        ReplayWavPack(io::Stream* stream, io::Stream* wvcStream, WavpackContext* context);

        static int32_t ReadBytes(void* id, void* data, int32_t bcount);
        static int PushBackByte(void* id, int c);
        static int64_t GetPos(void* id);
        static int SetPosAbs(void* id, int64_t pos);
        static int SetPosRel(void* id, int64_t delta, int mode);
        static int64_t GetLength(void* id);
        static int CanSeek(void* id);

    private:
        SmartPtr<io::Stream> m_stream;
        SmartPtr<io::Stream> m_wvcStream;
        WavpackContext* m_context;
        uint32_t m_bitrate = 0;

        static WavpackStreamReader64 ms_reader;
    };
}
// namespace rePlayer