#pragma once

#include <Replay.inl.h>

#include "core/sega.h"
#include "psflib/psflib.h"

namespace rePlayer
{
    class ReplayHighlyTheoretical : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        struct Settings : public Command<Settings, eReplay::HighlyTheoretical>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t numSongsMinusOne : 8;
                };
            };
            LoopInfo loops[0];

            static void Edit(ReplayMetadataContext& context);
        };

    private:
        static void* OpenPSF(void* context, const char* uri);
        static size_t ReadPSF(void* buffer, size_t size, size_t count, void* handle);
        static int SeekPSF(void* handle, int64_t offset, int whence);
        static int ClosePSF(void* handle);
        static long TellPSF(void* handle);

        static int InfoMetaPSF(void* context, const char* name, const char* value);

        static int SdsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size);

    public:
        ~ReplayHighlyTheoretical() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
        bool IsSeekable() const override { return false; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 44100;
        static constexpr uint32_t kDefaultSongDuration = 180 * 1000; // in milliseconds

        struct Subsong
        {
            uint32_t index = 0;
            uint32_t duration = 0;
            LoopInfo loop = {};
        };

    private:
        ReplayHighlyTheoretical(io::Stream* stream);
        ReplayHighlyTheoretical* Load(CommandBuffer metadata);

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

        uint8_t* m_segaState = nullptr;
        struct LoaderState
        {
            uint8_t* data = nullptr;
            size_t data_size = 0;

            void Clear() { if (data) free(data); new (this) LoaderState(); }

            ~LoaderState() { Clear(); }
        } m_loaderState = {};

        uint64_t m_length;
        std::string m_title;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        uint32_t m_currentSubsongIndex = 0xffFFffFF;

        uint8_t m_psfType = 0;
        bool m_hasLib = false;

        Array<Subsong> m_subsongs;
    };
}
// namespace rePlayer