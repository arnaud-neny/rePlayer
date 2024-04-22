#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

#include "zingzong/src/zingzong.h"

namespace rePlayer
{
    class ReplayQuartet : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::Quartet>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t overrideSurround : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t surround : 1;
                    uint32_t overrideRate : 1;
                    uint32_t rate : 10;
                };
            };
            uint32_t duration = 0;

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplayQuartet() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }
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

    private:
        static constexpr uint32_t kSampleRate = 48000;
        static constexpr uint32_t kDefaultSongDuration = 150000;

        struct FileHandle : public vfs_s
        {
            SmartPtr<io::Stream> stream;
            std::string uri;
        };

        struct Driver : public zz_vfs_dri_s
        {
            SmartPtr<io::Stream> stream;
        };

    private:
        ReplayQuartet(zz_play_t player, zz_u8_t format, CommandBuffer metadata);

        static zz_err_t vfs_reg(zz_vfs_dri_t);
        static zz_err_t vfs_unreg(zz_vfs_dri_t);
        static zz_u16_t vfs_ismine(const char*);
        static zz_vfs_t vfs_create(const char*, va_list);
        static void vfs_destroy(zz_vfs_t);
        static const char* vfs_uri(zz_vfs_t);
        static zz_err_t vfs_open(zz_vfs_t);
        static zz_err_t vfs_close(zz_vfs_t);
        static zz_u32_t vfs_read(zz_vfs_t, void*, zz_u32_t);
        static zz_u32_t vfs_tell(zz_vfs_t);
        static zz_u32_t vfs_size(zz_vfs_t);
        static zz_err_t vfs_seek(zz_vfs_t, zz_u32_t, zz_u8_t);

    private:
        zz_play_t m_player;
        zz_info_t m_info;
        Surround m_surround;
        uint32_t m_stereoSeparation = 100;
        uint32_t m_duration = 0;
        uint16_t m_rate = 0;

        static Driver ms_driver;

    public:
        struct Globals
        {
            int32_t stereoSeparation;
            int32_t surround;
        } static ms_globals;
    };
}
// namespace rePlayer