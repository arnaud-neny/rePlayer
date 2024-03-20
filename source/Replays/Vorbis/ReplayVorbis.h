#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>

#include "minivorbis/minivorbis.h"

namespace rePlayer
{
    class ReplayVorbis : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayVorbis() override;

        uint32_t GetSampleRate() const override { return m_vorbis->vi->rate; }
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
        std::string GetStreamingArtist() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

        bool CanLoop() const override { return false; }

    private:
        ReplayVorbis(io::Stream* stream, OggVorbis_File* vorbis);

        static size_t OnRead(void* ptr, size_t size, size_t nmemb, void* datasource);
        static int32_t OnSeek(void* datasource, ogg_int64_t offset, int whence);
        static long OnTell(void* datasource);

    private:
        OggVorbis_File* m_vorbis;
        SmartPtr<io::Stream> m_stream;
    };
}
// namespace rePlayer