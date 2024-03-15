#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.inl.h>
#include <Containers/SmartPtr.h>

extern "C" {
#include "vgmstream/src/vgmstream.h"
}

namespace rePlayer
{
    class ReplayVGMStream : public Replay
    {
        struct StreamFile : public STREAMFILE
        {
            SmartPtr<io::Stream> stream;
        };

    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    private:
        static size_t StreamFileRead(StreamFile* sf, uint8_t* dst, offv_t offset, size_t length);
        static size_t StreamFileGetSize(StreamFile* sf);
        static offv_t StreamFileGetOffset(StreamFile* sf);
        static void StreamFileGetName(StreamFile* sf, char* name, size_t name_size);
        static StreamFile* StreamFileOpen(StreamFile* sf, const char* const filename, size_t buf_size);
        static void StreamFileClose(StreamFile* sf);

    public:
        ~ReplayVGMStream() override;

        uint32_t GetSampleRate() const override { return uint32_t(m_vgmstream->sample_rate); }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        ReplayVGMStream(StreamFile* streamFile, VGMSTREAM* vgmstream, eExtension ext);

    private:
        StreamFile* m_streamFile;
        VGMSTREAM* m_vgmstream;
        double m_loopCount;
    };
}
// namespace rePlayer