#pragma once

#include <Replay.h>
#include <Audio/AudioTypes.h>
#include <Containers/SmartPtr.h>

#define DRFLAC_MALLOC(sz) core::Alloc((sz))
#define DRFLAC_REALLOC(p, sz) core::Realloc((p), (sz))
#define DRFLAC_FREE(p) core::Free((p))
#define DR_FLAC_NO_STDIO
#include <dr_flac.h>

namespace rePlayer
{
    class ReplayFLAC : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayFLAC() override;

        uint32_t GetSampleRate() const override { return m_flac->sampleRate; }
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

        bool CanLoop() const override { return false; }

    private:
        ReplayFLAC(io::Stream* stream);

        static size_t OnRead(void* pUserData, void* pBufferOut, size_t bytesToRead);
        static drflac_bool32 OnSeek(void* pUserData, int offset, drflac_seek_origin origin);
        static void OnMetadata(void* pUserData, drflac_metadata* pMetadata);

    private:
        SmartPtr<io::Stream> m_stream;
        drflac* m_flac;
        std::string m_info;
    };
}
// namespace rePlayer