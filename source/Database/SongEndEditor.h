#pragma once

#include <Audio/AudioTypes.h>
#include <Containers/Array.h>
#include <Thread/Semaphore.h>
#include <Database/Types/MusicID.h>

namespace rePlayer
{
    using namespace core;
    class BusySpinner;
    class Replay;
    struct ReplayMetadataContext;

    class SongEndEditor
    {
    public:
        static SongEndEditor* Create(ReplayMetadataContext& context, MusicID musicId);
        SongEndEditor(MusicID musicId, Replay* replay, LoopInfo loop);
        ~SongEndEditor();

        bool Update(ReplayMetadataContext& context);

    private:
        // frame is set of samples info per pixel
        struct Frame
        {
            uint8_t min;
            uint8_t max;
            uint8_t rms;
        };

        struct Wave;

    private:
        void Render();
        uint32_t ReadCurrentSample() const;
        void OpenAudio();

        bool IsBusy() const;

        void FindLoop();

        uint32_t WaveformUI();
        void PlaybackUI();
        void ScaleUI(uint32_t width);
        void LoopUI();
        void ParamsUI();

    private:
        MusicID m_musicId;
        Replay* m_replay;
        std::string m_title;

        Array<StereoSample> m_samples;
        Array<Frame> m_frames;
        Array<uint32_t> m_loops;
        uint64_t m_numMillisecondsPerPixel = 250;
        int64_t m_silence = -1;
        LoopInfo m_loop;
        uint32_t m_currentSample = 0;
        uint32_t m_currentFrameSample = 0;
        uint32_t m_numSamples = 0;
        bool m_isPlaying = false;
        bool m_isRunning = true;
        bool m_isJobDone = false;
        bool m_isWaveScrolling = false;
        float m_waveScroll = 0.0f;
        uint32_t m_waveStartPosition = 0;
        Wave* m_wave = nullptr;
        thread::Semaphore m_semaphore;

        // mouse & loop
        float m_mousePosWithLoop = 0.0f;
        LoopInfo m_mouseLoop;
        enum class EditLoop
        {
            None,
            Create,
            Move,
            ResizeLeft,
            ResizeRight
        } m_mouseMode = EditLoop::None;

#if !defined (WIN64) && !defined (_WIN64)
        static constexpr uint32_t kMinDownsampleFactor = 8; // memory issue on win32
#else
        static constexpr uint32_t kMinDownsampleFactor = 1;
#endif

        struct
        {
            uint32_t downsampleFactor = kMinDownsampleFactor;
            uint32_t loopMin = 10;
            uint32_t loopMax = 180;
            float peakThreshold = 0.58f; // 0.45-0.65
            float consistencyThreshold = 0.45f;//0.35-0.55

            int32_t frame = 1024;
            int32_t hop = 128;
            int32_t melFilters = 26;
            int32_t mfccs = 13;
        } m_loopDetection;
        SmartPtr<BusySpinner> m_busySpinner;
    };
}
// namespace rePlayer