#pragma once

#include <Replay.inl.h>
#include <Audio/Surround.h>

typedef struct _sc68_s sc68_t;

namespace rePlayer
{
    class ReplaySC68 : public Replay
    {
    public:
        static bool Init(SharedContexts* ctx, Window& window);
        static void Release();

        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

        static bool DisplaySettings();

        struct Settings : public Command<Settings, eReplay::sc68>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                };
            };

            static void Edit(ReplayMetadataContext& context);
        };

    public:
        ~ReplaySC68() override;

        uint32_t GetSampleRate() const override { return kSampleRate; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        static constexpr uint32_t kSampleRate = 48000;

    private:
        ReplaySC68(sc68_t* sc68Instance0, sc68_t* sc68Instance1);

        static void ApplySettings();
        static eExtension GetExtension(sc68_t* sc68Instance);

    private:
        sc68_t* m_sc68Instances[2];
        Surround m_surround;
        bool m_hasLooped = false;

        static int32_t ms_aSIDfier;
        //static int32_t ms_defaultTime;
        static int32_t ms_ymEngineAndFilter;
        static int32_t ms_ymVolumeModel;
        //static int32_t ms_paulaClock;
        static int32_t ms_paulaFilter;
        static int32_t ms_paulaBlending;
        static int32_t ms_surround;
    };
}
// namespace rePlayer