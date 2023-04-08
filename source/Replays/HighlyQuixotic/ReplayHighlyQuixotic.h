#pragma once

#include <Replay.h>
#include <Containers/Array.h>
#include "psflib/psflib.h"

namespace rePlayer
{
    class ReplayHighlyQuixotic : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    private:
        static void* OpenPSF(void* context, const char* uri);
        static size_t ReadPSF(void* buffer, size_t size, size_t count, void* handle);
        static int SeekPSF(void* handle, int64_t offset, int whence);
        static int ClosePSF(void* handle);
        static long TellPSF(void* handle);

        static int InfoMetaPSF(void* context, const char* name, const char* value);

        static int QsoundLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size);

    public:
        ~ReplayHighlyQuixotic() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint16_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 24038;
        static constexpr uint32_t kDefaultLength = 180 * 1000;

    private:
        ReplayHighlyQuixotic(io::Stream* stream);
        ReplayHighlyQuixotic* Load();

    private:
        SmartPtr<io::Stream> m_streamArchive;
        SmartPtr<io::Stream> m_stream;
        const psf_file_callbacks m_psfFileSystem = {
            "\\/|:",
            this,
            OpenPSF,
            ReadPSF,
            SeekPSF,
            ClosePSF,
            TellPSF
        };

        Array<std::string> m_tags;

        uint8_t* m_qsoundState = nullptr;

        struct valid_range
        {
            uint32_t start;
            uint32_t size;
        };

        Array<uint8_t> m_aKey;
        Array<valid_range> m_aKeyValid;
        Array<uint8_t> m_aZ80ROM;
        Array<valid_range> m_aZ80ROMValid;
        Array<uint8_t> m_aSampleROM;
        Array<valid_range> m_aSampleROMValid;

        uint32_t m_length = kDefaultLength;
        std::string m_title;
        uint64_t m_currentPosition = 0;
        uint64_t m_globalPosition = 0;
        uint32_t m_currentSubsongIndex = 0xffFFffFF;

        Array<uint32_t> m_subsongs;
    };
}
// namespace rePlayer