#pragma once

#include <Replay.h>
#include <Containers/Array.h>
#include "psflib/psflib.h"
#include <mgba/core/core.h>

namespace rePlayer
{
    class ReplayHighlyAdvanced : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        struct Settings : public Command<Settings, eReplay::HighlyAdvanced>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t numSongsMinusOne : 8;
                };
            };
            uint32_t* GetDurations() { return reinterpret_cast<uint32_t*>(this + 1); }

            static void Edit(ReplayMetadataContext& context);
        };

    private:
        static void* OpenPSF(void* context, const char* uri);
        static size_t ReadPSF(void* buffer, size_t size, size_t count, void* handle);
        static int SeekPSF(void* handle, int64_t offset, int whence);
        static int ClosePSF(void* handle);
        static long TellPSF(void* handle);

        static int InfoMetaPSF(void* context, const char* name, const char* value);

        static int GsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size);

    public:
        ~ReplayHighlyAdvanced() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000/*32768*/;
        static constexpr uint32_t kDefaultSongDuration = 180 * 1000; // in milliseconds

        struct Subsong
        {
            uint32_t index = 0;
            uint32_t duration = 0;
            uint32_t overriddenDuration = 0;
        };

    private:
        ReplayHighlyAdvanced(io::Stream* stream);
        ReplayHighlyAdvanced* Load(CommandBuffer metadata);

        void SetupMetadata(CommandBuffer metadata);

    private:
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

        struct gsf_loader_state
        {
            int entry_set = 0;
            uint32_t entry = 0;
            uint8_t* data = nullptr;
            size_t data_size = 0;
            ~gsf_loader_state() { if (data) free(data); }
        } m_gbaRom;
        mCore* m_gbaCore = nullptr;
        struct GbaStream : public mAVStream
        {
            int16_t samples[2048 * 2];
            uint32_t numSamples;
        } m_gbaStream;

        uint64_t m_length;
        std::string m_title;
        uint64_t m_globalPosition = 0;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        uint32_t m_currentSubsongIndex = 0xffFFffFF;

        bool m_hasLib = false;

        Array<Subsong> m_subsongs;
    };
}
// namespace rePlayer