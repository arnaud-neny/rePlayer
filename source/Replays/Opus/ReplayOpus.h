#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>

#include <opusfile.h>

namespace rePlayer
{
    class ReplayOpus : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayOpus() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
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
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplayOpus(OggOpusFile* opus, io::Stream* stream);

        static int32_t OnRead(io::Stream* stream, uint8_t* ptr, int32_t nbytes);
        static opus_int64 OnSeek(io::Stream* stream, opus_int64 offset, int32_t whence);
        static opus_int64 OnTell(io::Stream* stream);

    private:
        OggOpusFile* m_opus;
        SmartPtr<io::Stream> m_stream;

        StereoSample m_samples[120 * 48];
        uint32_t m_remainingSamples = 0;
        uint32_t m_numSamples = 0;

        int32_t m_previousLink = -1;
        uint8_t m_numChannels = 0;
        uint32_t m_originalSampleRate = 0;
        uint32_t m_bitRate = 0;

        std::string m_metadata;
        std::string m_artists;
        std::string m_title;
    };
}
// namespace rePlayer