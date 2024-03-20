#pragma once

#include <Replay.inl.h>

#include "psflib/psflib.h"

namespace rePlayer
{
    class ReplayHighlyCompetitive : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::HighlyCompetitive>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t numSongsMinusOne : 8;
                    uint32_t overrideInterpolation : 1;
                    uint32_t interpolation : 3;
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

        static int SnsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* reserved, size_t reserved_size);
        int SnsfLoadMap(int issave, const unsigned char* udata, unsigned usize);
        int SnsfLoadMapz(int issave, const unsigned char* zdata, unsigned zsize, unsigned zcrc);

    public:
        ~ReplayHighlyCompetitive() override;

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
        static constexpr uint32_t kSampleRate = 48000/*32768*/;
        static constexpr uint32_t kDefaultSongDuration = 180 * 1000; // in milliseconds

        struct Subsong
        {
            uint32_t index = 0;
            uint32_t duration = 0;
            uint32_t overriddenDuration = 0;
        };

    private:
        ReplayHighlyCompetitive(io::Stream* stream);
        ReplayHighlyCompetitive* Load(CommandBuffer metadata);

        void SetupMetadata(CommandBuffer metadata);

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

        struct LoaderState
        {
            uint8_t* rom = nullptr;
            size_t romSize = 0;
            uint8_t* sram = nullptr;
            size_t sramSize = 0;

            void Clear() { if (rom) free(rom); if (sram) free(sram); new (this) LoaderState(); }

            ~LoaderState() { Clear(); }
        } m_loaderState = {};

        uint64_t m_length;
        std::string m_title;
        uint64_t m_currentPosition = 0;
        uint64_t m_currentDuration = 0;
        uint32_t m_currentSubsongIndex = 0xffFFffFF;

        bool m_hasLib = false;

        Array<Subsong> m_subsongs;

    public:
        static int32_t ms_interpolation;
    };
}
// namespace rePlayer